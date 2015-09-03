#!/usr/bin/perl
#
# Original script by Gokul Subramanian Ravi, 9/17/2013
#
#*****************************************************
# Enhanced by:
# Lokesh Jindal
# lokeshjindal15@cs.wisc.edu
# March, 2015
#*****************************************************

=pod

=head1 NAME

Script to parse multiple stats.txt files in a directory hierarchy for runs with DVFS

=head1 SYNOPSIS

This script is used to gather per-cpu/per node stats for mutiple benchmarks run with DVFS - essentially for runs
that dump stats regularly in stats.txt.
This script creates a per-core csv file cum.coreN.csv which contains FU utilization, core frequency, system-ethernet bytes
plotted against cumulative simulation time.

perl pd5_parse_stats.pl <dirname>

=head1 OPTIONS

=over 10

=item B<-help>

Usage: perl <script> <name of directory containing subdirectories with stats.txt files>

=item B<-dummy>

This is dummy help option

=back

=cut


use Getopt::Long;
use Pod::Usage;
use File::Find;
use File::Basename;
use Scalar::Util qw(reftype);

######################################################
# Set the following parameters that will be used to calculate
# utilization of FUs
my $IntPhyUnits = 3 ; # NEHALEM generally $IntPhyUnits == $IntAluUnits since every IntPhyUnit would be capable of doing IntAlut op and some other Int Op
my $FloatPhyUnits = 3; # NEHALEM don't rely on this generally $FloatPhyUnits == $FloatUnits
my $MemPhyUnits = 2; # NEHALEM generally $MemPhyUnits == $MemUnits

my $IntAluUnits = 1;
my $IntAluLatencyFac = 1;
my $IntMultUnits = 1;# no. of physical FUs capable of doing IntMult
my $IntMultLatencyFac = 3; # scaling factor to account for multicycle op; decided by OpCalss's issue latency and Op latency
my $IntDivUnits = 1;
my $IntDivLatencyFac = 20;

my $FloatUnits = 1; # our benchmarks are not FP intensive so let's take an average number for overall latency
my $FloatLatencyFac = 8;# this will be 

# Calculations below ignore the Simd insts
my $SimdUnits = 1; # our benchmarks are not SimD intensive so let's take an average number for overall latency
my $SimdLatencyFac = 10; #  

my $MemUnits = 1;
my $MemLatencyFac = 2;
my $SYS_ETH_MAX_BW = 160000000 * 2; # 160Mbps
#######################################################

my $test_name   = 'test';
my $output_dir_name = 'm5out';
my $output_dir_var;
my $help;

my $Vth_min    = 0.10;
my $Vth_max    = 0.21; 
my $Vth_step   = 0.01;
my $Tox_min    = 1.0;
my $Tox_max    = 1.5;
my $Tox_step   = 0.1;
my $Ldraw_min  = 32;
my $Ldraw_max  = 37;
my $Ldraw_step =  1;
my $Leff_min   = 7.4;
my $Leff_step  = 1.3;
my $Leff_max   = 18;

my $mLeff;
my $mVth;
my $mTox;
my $mLdraw;
my $mVth100;
my $mLeff10;
my $mLdraw10;
my $mTox10;
my $model_name;
my $model_name_var;
my $delay;
my $freq;

$result = GetOptions ("test=s" => \$test_name,    	# string
		"output=s"   => \$output_dir_name,      # string
		"ethbw=i"   => \$SYS_ETH_MAX_BW,      # integer
		"help|h" => \$help);  			# help

pod2usage(-1) if $help;

if ( $SYS_ETH_MAX_BW != (160000000 * 2))
{
	$SYS_ETH_MAX_BW = $SYS_ETH_MAX_BW * 1000000 * 2;
}
print "******** SYS_ETH_MAX_BW is $SYS_ETH_MAX_BW bps*************\n";
####################################################################

my $DATA_DIR = "";
if ($ARGV[0])
{
	$DATA_DIR = $ARGV[0];
	print "DATA_DIR being used is *$DATA_DIR*\n";
}
else
{
	print "Usage: <script> <name of directory containing subdirectories with stats.txt files>\n";
	print "Kindly enter a valid DATA_DIR to look for stats.txt files! Exiting ... \n";
	exit;
}

my @stats_file_list;

sub wanted_files
{
	my $file_name = $File::Find::name;
	#print "file_name is $file_name\n";
	if (!(-d $file_name))
	{
		print "file_name is $file_name\n";
	}

	if (!(-d $file_name) and ($_ =~ /stats.txt$/)and !($file_name =~ /switch/))# ignore switch stats.txt
	{
		push @stats_file_list, $file_name;
	}
}
find(\&wanted_files, $DATA_DIR);

$num_stats_file = @stats_file_list;



print "number of stats files found is $num_stats_file\n";
my $l = 0;
while ( $l < $num_stats_file)
{
	chomp($stats_file_list[$l]);
	print "$stats_file_list[$l]\n";
	$l++;
}

my %EXEC_TIME_LIST;
my %ENERGY_LIST;
my %EDP_LIST;
my %core0_time_at_freq;
my %core1_time_at_freq;
my %core2_time_at_freq;
my %core3_time_at_freq;
my %core0_PCtime_at_freq;
my %core1_PCtime_at_freq;
my %core2_PCtime_at_freq;
my %core3_PCtime_at_freq;
my @BENCHMARKS;

my $STATS_FILE_I = 0;
while ($STATS_FILE_I < $num_stats_file)
{
	$stats_file = $stats_file_list[$STATS_FILE_I];
	$out_dir = dirname($stats_file);
	$BENCHMARKS[$STATS_FILE_I] = basename ($out_dir);
	print "Stats file beng used is $stats_file and outdir is $out_dir\n";
	print "********DANGER******** Removing existing part-wise files: rm $out_dir/stats.txt.*\n";
	`rm $out_dir/stats.txt.`; 	
	
	my $MAXPHASES = 0;
	$MAXPHASES = `grep "Begin Simulation Statistics" $stats_file | wc -l`;
	chomp($MAXPHASES);
	print "Number of phases in $stats_file is $MAXPHASES\n";
	
	open STATS_FILE, $stats_file or die $!;
	my $line="";
	my $part = 0;
	my @proc_sim_seconds;
	my @sim_ticks;
	my $proc_total_sim_seconds=0;
	my @proc_clk;
	my @proc_freq;
	my @proc_int_busy_cycles;
	my @proc_num_cycles;
	my @proc_int_max_cycles;
	my @proc_int_util;
	my @proc_mem_busy_cycles;
	my @proc_mem_max_cycles;
	my @proc_mem_util;
	my @proc_float_busy_cycles;
	my @proc_float_max_cycles;
	my @proc_float_util;
	my $tmp_sim_seconds = 0;
	my $captured = 1;
	while ($line = <STATS_FILE>)
	{
		if ($line =~ /.*Begin Simulation Statistics.*/)
		{
			#print "Detected Begin\n";	
			if ($part)
			{
			# close OUTFILE;
			}
			$part++;

			#initialize all values to be grepped to -1
			my $num = 0;
			while ($num < 4)
			{
			$proc_freq[$num][$part -1] = -1;
			$proc_int_busy_cycles[$num][$part -1] = -1;
			$proc_float_busy_cycles[$num][$part -1] = -1;
			$proc_mem_busy_cycles[$num][$part -1] = -1;
			
			$num++;
			}

			# print "Creating file $stats_file.$part\n";
			# open OUTFILE, "> $stats_file.$part";
			# print OUTFILE $line;
		}
	
		else
		{
			if ($line =~ /.*sim_seconds\s+(\d+\.?\d+).*/)
			{
				if ($captured eq 0)
				{
					print "\n***** ERROR!!! CAPTURED is ZERO = $captured for PART = $part\n";
					exit;
				}
				$tmp_sim_seconds = $1;
				#print "0 tmp_sim_seconds is $tmp_sim_seconds and part is $part\n";
				$captured = 0;
				#$proc_sim_seconds{$part} = $1;
				#print "proc_sim_seconds $proc_sim_seconds{$part}\n";
				#$proc_total_sim_seconds += $proc_sim_seconds{$part};
			}
			if ($line =~ /.*sim_ticks\s+(\d+)/)
			{
				$sim_ticks[$part -1] = $1; # 1 sim_tick is 10^(-12)s
			}
			if ($line =~ /.*cpu_clk_domain.clock\s+(\d+)/)
			{
				# calculate no. of cycles simulated for this processor
				$proc_num_cycles[0][$part - 1] = $sim_ticks[$part - 1] / $1; # since 1 $1 gives no. of ticks in 1 cpu cycle
				print "proc_num_cycles cpu0: $proc_num_cycles[0][$part - 1]\n";

				$proc_clk[0][$part - 1] = $1;
				#print "proc_clk $proc_clk[$part - 1]\n";
				$proc_freq[0][$part -1] = 1000/$proc_clk[0][$part -1];#in GHz
				$proc_freq[0][$part -1] = int($proc_freq[0][$part -1] * 1000);#in MHz
				#print "1 tmp_sim_seconds is $tmp_sim_seconds and part is $part\n";
				#$proc_sim_seconds[$part -1] = $tmp_sim_seconds;
				#$proc_total_sim_seconds += $proc_sim_seconds[$part -1];
			}
			if ($line =~ /.*cpu_clk_domain1.clock\s+(\d+)/)
			{
				$proc_num_cycles[1][$part - 1] = $sim_ticks[$part - 1] / $1; # since 1 $1 gives no. of ticks in 1 cpu cycle
				print "proc_num_cycles cpu1: $proc_num_cycles[1][$part - 1]\n";

				$proc_clk[1][$part - 1] = $1;
				#print "proc_clk $proc_clk[$part - 1]\n";
				$proc_freq[1][$part -1] = 1000/$proc_clk[1][$part -1];#in GHz
				$proc_freq[1][$part -1] = int($proc_freq[1][$part -1] * 1000);#in MHz
				#print "1 tmp_sim_seconds is $tmp_sim_seconds and part is $part\n";
				#$proc_sim_seconds[$part -1] = $tmp_sim_seconds;
				#$proc_total_sim_seconds += $proc_sim_seconds[$part -1];
			}
			if ($line =~ /.*cpu_clk_domain2.clock\s+(\d+)/)
			{
				$proc_num_cycles[2][$part - 1] = $sim_ticks[$part - 1] / $1; # since 1 $1 gives no. of ticks in 1 cpu cycle
				print "proc_num_cycles cpu2: $proc_num_cycles[2][$part - 1]\n";

				$proc_clk[2][$part - 1] = $1;
				#print "proc_clk $proc_clk[$part - 1]\n";
				$proc_freq[2][$part -1] = 1000/$proc_clk[2][$part -1];#in GHz
				$proc_freq[2][$part -1] = int($proc_freq[2][$part -1] * 1000);#in MHz
				#print "1 tmp_sim_seconds is $tmp_sim_seconds and part is $part\n";
				#$proc_sim_seconds[$part -1] = $tmp_sim_seconds;
				#$proc_total_sim_seconds += $proc_sim_seconds[$part -1];
			}
			if ($line =~ /.*cpu_clk_domain3.clock\s+(\d+)/)
			{
				$proc_num_cycles[3][$part - 1] = $sim_ticks[$part - 1] / $1; # since 1 $1 gives no. of ticks in 1 cpu cycle
				print "proc_num_cycles cpu3: $proc_num_cycles[3][$part - 1]\n";

				$proc_clk[3][$part - 1] = $1;
				#print "proc_clk $proc_clk[$part - 1]\n";
				$proc_freq[3][$part -1] = 1000/$proc_clk[3][$part -1];#in GHz
				$proc_freq[3][$part -1] = int($proc_freq[3][$part -1] * 1000);#in MHz
				#print "1 tmp_sim_seconds is $tmp_sim_seconds and part is $part\n";
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::IntAlu\s+(\d+)/)
			{
				print "intalu_insts: cpu$1: $2\n";
				#initialize to zero
				$proc_int_busy_cycles[$1][$part - 1] = 0;
				$proc_int_busy_cycles[$1][$part - 1] += $2 * $IntAluLatencyFac;
				print "proc_int_busy_cycles cpu$1 = $proc_int_busy_cycles[$1][$part - 1]\n";
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::IntMult\s+(\d+)/)
			{
				print "intmult_insts: cpu$1: $2\n";
				$proc_int_busy_cycles[$1][$part - 1] += $2 * $IntMultLatencyFac;
				print "proc_int_busy_cycles = $proc_int_busy_cycles[$1][$part - 1]\n";
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::IntDiv\s+(\d+)/)
			{
				print "intdiv_insts: cpu$1: $2\n";
				$proc_int_busy_cycles[$1][$part - 1] += $2 * $IntDivLatencyFac;
				print "proc_int_busy_cycles = $proc_int_busy_cycles[$1][$part - 1]\n";
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::FloatAdd\s+(\d+)/)
			{
				print "floatadd_insts: cpu$1: $2\n";
				# initialize to zero
				$proc_float_busy_cycles[$1][$part - 1] = 0;
				$proc_float_busy_cycles[$1][$part - 1] += $2 * $FloatLatencyFac;
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::FloatCmp\s+(\d+)/)
			{
				print "floatcmp_insts: cpu$1: $2\n";
				$proc_float_busy_cycles[$1][$part - 1] += $2 * $FloatLatencyFac;
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::FloatCvt\s+(\d+)/)
			{
				print "floatcvt_insts: cpu$1: $2\n";
				$proc_float_busy_cycles[$1][$part - 1] += $2 * $FloatLatencyFac;
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::FloatMult\s+(\d+)/)
			{
				print "floatmult_insts: cpu$1: $2\n";
				$proc_float_busy_cycles[$1][$part - 1] += $2 * $FloatLatencyFac;
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::FloatDiv\s+(\d+)/)
			{
				print "floatdiv_insts: cpu$1: $2\n";
				$proc_float_busy_cycles[$1][$part - 1] += $2 * $FloatLatencyFac;
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::FloatSqrt\s+(\d+)/)
			{
				print "floatsqrt_insts: cpu$1: $2\n";
				$proc_float_busy_cycles[$1][$part - 1] += $2 * $FloatLatencyFac;
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::MemRead\s+(\d+)/)
			{
				print "memread_insts: cpu$1: $2\n";
				# init to zero
				$proc_mem_busy_cycles[$1][$part - 1] = 0;
				$proc_mem_busy_cycles[$1][$part - 1] += $2 * $MemLatencyFac;
			}
			if ($line =~ /.*system.switch_cpus(\d).iq.FU_type_0::MemWrite\s+(\d+)/)
			{
				print "memwrite_insts: cpu$1: $2\n";
				$proc_mem_busy_cycles[$1][$part - 1] += $2 * $MemLatencyFac;
			}
			if ($line =~ /End Simulation Statistics/)
			{
				if ($captured eq 1)
				{
					print "\n***** ERROR!!! CAPTURED is ONE = $captured for PART = $part\n";
					exit;
				}
				$captured = 1;
				my $num = 0;
				while ($num < 4)
				{
				print "captures has been anded with :$proc_freq[$num][$part -1]:*:$proc_int_busy_cycles[$num][$part -1]:*:$proc_float_busy_cycles[$num][$part -1]:*:$proc_mem_busy_cycles[$num][$part -1]:\n";
				$captured &= ($proc_freq[$num][$part -1] != -1);
				$captured &= ($proc_int_busy_cycles[$num][$part -1] != -1);
				$captured &= ($proc_float_busy_cycles[$num][$part -1] != -1);
				$captured &= ($proc_mem_busy_cycles[$num][$part -1] != -1);
				
				$num++;
				}
				# let's calculate the per-cpu FU utilization for this phase/part
				$num = 0;
				while ($num < 4)
				{
				$proc_int_max_cycles[$num][$part -1] = $proc_num_cycles[$num][$part - 1] * $IntPhyUnits; 
				$proc_float_max_cycles[$num][$part -1] = $proc_num_cycles[$num][$part - 1] * $FloatPhyUnits; 
				$proc_mem_max_cycles[$num][$part -1] = $proc_num_cycles[$num][$part - 1] * $MemPhyUnits; 
				$proc_int_util[$num][$part - 1] = sprintf("%.2f",$proc_int_busy_cycles[$num][$part-1] / $proc_int_max_cycles[$num][$part -1] * 100);
				$proc_float_util[$num][$part - 1] = sprintf("%.2f",$proc_float_busy_cycles[$num][$part-1] / $proc_float_max_cycles[$num][$part -1] * 100) ;
				$proc_mem_util[$num][$part - 1] = sprintf("%.2f",$proc_mem_busy_cycles[$num][$part-1] / $proc_mem_max_cycles[$num][$part -1] * 100) ;
				
				$num++;
				}
				
				#let's store the sim_seconds in this phase
				$proc_sim_seconds[$part -1] = $tmp_sim_seconds;
				$proc_total_sim_seconds += $proc_sim_seconds[$part -1];

			}
			
			# print OUTFILE $line;
		}
	}
	
	# close OUTFILE;
	
	#Do not want to count the last part of stats.txt
	$proc_total_sim_seconds -= $proc_sim_seconds[$part -1];
	
	if ($part != $MAXPHASES)
	{
		print "***** ERROR!!! part = $part and MAXPHASES = $MAXPHASES not equal\n";
		exit;
	}
	print "Done splitting $stats_file into $part files!!!!\n\n";
	print "*******************************************************************************************\n";
        
        # calculate cumulative time for each frequency  
        $phase = 0 ; 
        foreach my $f (@{ $proc_freq[0] })
        {
            if (exists $core0_time_at_freq{$f})
            {
                $core0_time_at_freq{$f} += $proc_sim_seconds[$phase];
            }
            else
            {
                $core0_time_at_freq{$f} = $proc_sim_seconds[$phase];
            }

            $phase++;
        }
	if ($phase != $MAXPHASES)
	{
		print "***** CORE0 ERROR!!! phase = $phase and MAXPHASES = $MAXPHASES not equal\n";
		exit;
	}
        $phase = 0 ; 
        foreach my $f (@{ $proc_freq[1] })
        {
            if (exists $core1_time_at_freq{$f})
            {
                $core1_time_at_freq{$f} += $proc_sim_seconds[$phase];
            }
            else
            {
                $core1_time_at_freq{$f} = $proc_sim_seconds[$phase];
            }

            $phase++;
        }
	if ($phase != $MAXPHASES)
	{
		print "***** CORE1 ERROR!!! phase = $phase and MAXPHASES = $MAXPHASES not equal\n";
		exit;
	}
        $phase = 0 ; 
        foreach my $f (@{ $proc_freq[2] })
        {
            if (exists $core2_time_at_freq{$f})
            {
                $core2_time_at_freq{$f} += $proc_sim_seconds[$phase];
            }
            else
            {
                $core2_time_at_freq{$f} = $proc_sim_seconds[$phase];
            }

            $phase++;
        }
	if ($phase != $MAXPHASES)
	{
		print "***** CORE2 ERROR!!! phase = $phase and MAXPHASES = $MAXPHASES not equal\n";
		exit;
	}
        $phase = 0 ; 
        foreach my $f (@{ $proc_freq[3] })
        {
            if (exists $core3_time_at_freq{$f})
            {
                $core3_time_at_freq{$f} += $proc_sim_seconds[$phase];
            }
            else
            {
                $core3_time_at_freq{$f} = $proc_sim_seconds[$phase];
            }

            $phase++;
        }
	if ($phase != $MAXPHASES)
	{
		print "***** CORE3 ERROR!!! phase = $phase and MAXPHASES = $MAXPHASES not equal\n";
		exit;
	}

        # populate the missing frequencies with zeros
        @freq_possible = (200.0, 300.0, 400.0, 500.0, 599.0, 699.0, 800.0, 900.0, 1000.0, 1100.0, 1200.0, 1300.0, 1400.0);

        foreach my $f ( @freq_possible )
        {
            if (!(exists $core0_time_at_freq{$f}))
            {   $core0_time_at_freq{$f} = 0; }
            if (!(exists $core1_time_at_freq{$f}))
            {   $core1_time_at_freq{$f} = 0; }
            if (!(exists $core2_time_at_freq{$f}))
            {   $core2_time_at_freq{$f} = 0; }
            if (!(exists $core3_time_at_freq{$f}))
            {   $core3_time_at_freq{$f} = 0; }
        }

        # calculate % time spent in each frequency by each core
        foreach $f (keys %core0_time_at_freq)
        {
            $core0_PCtime_at_freq{$f} = ($core0_time_at_freq{$f} / $proc_total_sim_seconds) * 100;
        }
        foreach $f (keys %core1_time_at_freq)
        {
            $core1_PCtime_at_freq{$f} = ($core1_time_at_freq{$f} / $proc_total_sim_seconds) * 100;
        }
        foreach $f (keys %core2_time_at_freq)
        {
            $core2_PCtime_at_freq{$f} = ($core2_time_at_freq{$f} / $proc_total_sim_seconds) * 100;
        }
        foreach $f (keys %core3_time_at_freq)
        {
            $core3_PCtime_at_freq{$f} = ($core3_time_at_freq{$f} / $proc_total_sim_seconds) * 100;
        }

	############################################
	##now use the spice results to obtain the proc power values by scaling
	###########################################
	#### create csv for time vs frequency
	my $iter = 0;
	my $cum_proc_sim_seconds = 0;
	my $plot_sim_sec = 0;
	my $totalp = 0;
	
	my @allcores_freq;	
	my @allcores_intutil;	
	my @allcores_floatutil;	
	my @allcores_memutil;	

	open F10, ">$out_dir/timefreq.csv" or die $!;
	print F10 "SimSec,Core0Freq,Core1Freq,Core2Freq,Core3Freq\n";
	open F0, ">$out_dir/cum.core0.csv" or die $!;
	print F0 "CumSimSec,Core0Freq, ,IntUtil,FloatUtil,MemUtil, ,SimSec\n";
	open F1, ">$out_dir/cum.core1.csv" or die $!;
	print F1 "CumSimSec,Core1Freq, ,IntUtil,FloatUtil,MemUtil, ,SimSec\n";
	open F2, ">$out_dir/cum.core2.csv" or die $!;
	print F2 "CumSimSec,Core2Freq, ,IntUtil,FloatUtil,MemUtil, ,SimSec\n";
	open F3, ">$out_dir/cum.core3.csv" or die $!;
	print F3 "CumSimSec,Core3Freq, ,IntUtil,FloatUtil,MemUtil, ,SimSec\n";
	open F4, ">$out_dir/cum.allcores.csv" or die $!;
	print F4 "CumSimSec,Freq, ,IntUtil,FloatUtil,MemUtil, ,SimSec\n";
	
	#open F6, ">$out_dir/cum.timedynp.csv" or die $!;
	#print F6 "cum_proc_sim_seconds,proc_dynp0,proc_dynp1,proc_dynp2,proc_dynp3\n";
	while($iter < ($MAXPHASES -1)) # TODO FIXME you need to change this depending on wthether you are dumping and resetting stats immediately after executing benchmark
        # while($iter < ($MAXPHASES))
	{	
		#$totalp = $proc_dynp[$iter] + $proc_leakp[$iter];
		#$totalp = ($proc_leakp[0][$iter] + $proc_leakp[1][$iter] + $proc_leakp[2][$iter] +$proc_leakp[3][$iter] 
		#				+ $proc_dynp[0][$iter] + $proc_dynp[1][$iter] + $proc_dynp[2][$iter] + $proc_dynp[3][$iter]
		#				+ $DIRECTORY_POWER + $NOC_POWER);
		# print F10 "$proc_sim_seconds[$iter],$proc_freq[0][$iter],$proc_freq[1][$iter],$proc_freq[2][$iter],$proc_freq[3][$iter]\n";
		# $plot_sim_sec = $cum_proc_sim_seconds + 0.00001;
		# print F0 "$plot_sim_sec,$proc_freq[0][$iter], ,$proc_int_util[0][$iter],$proc_float_util[0][$iter],$proc_mem_util[0][$iter], ,$sys_eth_bytes[$iter],$sys_eth_bw[$iter],$sys_eth_totbw[$iter],$proc_sim_seconds[$iter]\n";
		# print F1 "$plot_sim_sec,$proc_freq[1][$iter], ,$proc_int_util[1][$iter],$proc_float_util[1][$iter],$proc_mem_util[1][$iter], ,$sys_eth_bytes[$iter],$sys_eth_bw[$iter],$sys_eth_totbw[$iter],$proc_sim_seconds[$iter]\n";
		# print F2 "$plot_sim_sec,$proc_freq[2][$iter], ,$proc_int_util[2][$iter],$proc_float_util[2][$iter],$proc_mem_util[2][$iter], ,$sys_eth_bytes[$iter],$sys_eth_bw[$iter],$sys_eth_totbw[$iter],$proc_sim_seconds[$iter]\n";
		# print F3 "$plot_sim_sec,$proc_freq[3][$iter], ,$proc_int_util[3][$iter],$proc_float_util[3][$iter],$proc_mem_util[3][$iter], ,$sys_eth_bytes[$iter],$sys_eth_bw[$iter],$sys_eth_totbw[$iter],$proc_sim_seconds[$iter]\n";
		
		$allcores_freq[$iter] = ($proc_freq[0][$iter] + $proc_freq[1][$iter] + $proc_freq[2][$iter] + $proc_freq[3][$iter])/4;
		$allcores_intutil[$iter] = ($proc_int_util[0][$iter] + $proc_int_util[1][$iter] + $proc_int_util[2][$iter] + $proc_int_util[3][$iter])/4;
		$allcores_floatutil[$iter] = ($proc_float_util[0][$iter] + $proc_float_util[1][$iter] + $proc_float_util[2][$iter] + $proc_float_util[3][$iter])/4;
		$allcores_memutil[$iter] = ($proc_mem_util[0][$iter] + $proc_mem_util[1][$iter] + $proc_mem_util[2][$iter] + $proc_mem_util[3][$iter])/4;

		# print F4 "$plot_sim_sec,$allcores_freq[$iter], ,$allcores_intutil[$iter],$allcores_floatutil[$iter],$allcores_memutil[$iter], ,$sys_eth_bytes[$iter],$sys_eth_bw[$iter],$sys_eth_totbw[$iter],$proc_sim_seconds[$iter]\n";
		
		#print F6 "$plot_sim_sec,$proc_dynp[0][$iter],$proc_dynp[1][$iter],$proc_dynp[2][$iter],$proc_dynp[3][$iter]\n";
                $dummy_cum_proc_sim_seconds = $cum_proc_sim_seconds + 0.000001;
		print F0 "$dummy_cum_proc_sim_seconds,$proc_freq[0][$iter], ,$proc_int_util[0][$iter],$proc_float_util[0][$iter],$proc_mem_util[0][$iter], ,$proc_sim_seconds[$iter]\n";
		print F1 "$dummy_cum_proc_sim_seconds,$proc_freq[1][$iter], ,$proc_int_util[1][$iter],$proc_float_util[1][$iter],$proc_mem_util[1][$iter], ,$proc_sim_seconds[$iter]\n";
		print F2 "$dummy_cum_proc_sim_seconds,$proc_freq[2][$iter], ,$proc_int_util[2][$iter],$proc_float_util[2][$iter],$proc_mem_util[2][$iter], ,$proc_sim_seconds[$iter]\n";
		print F3 "$dummy_cum_proc_sim_seconds,$proc_freq[3][$iter], ,$proc_int_util[3][$iter],$proc_float_util[3][$iter],$proc_mem_util[3][$iter], ,$proc_sim_seconds[$iter]\n";
		
		print F4 "$dummy_cum_proc_sim_seconds,$allcores_freq[$iter], ,$allcores_intutil[$iter],$allcores_floatutil[$iter],$allcores_memutil[$iter], ,$proc_sim_seconds[$iter]\n";
		


		$cum_proc_sim_seconds += $proc_sim_seconds[$iter];
		print F0 "$cum_proc_sim_seconds,$proc_freq[0][$iter], ,$proc_int_util[0][$iter],$proc_float_util[0][$iter],$proc_mem_util[0][$iter], ,$proc_sim_seconds[$iter]\n";
		print F1 "$cum_proc_sim_seconds,$proc_freq[1][$iter], ,$proc_int_util[1][$iter],$proc_float_util[1][$iter],$proc_mem_util[1][$iter], ,$proc_sim_seconds[$iter]\n";
		print F2 "$cum_proc_sim_seconds,$proc_freq[2][$iter], ,$proc_int_util[2][$iter],$proc_float_util[2][$iter],$proc_mem_util[2][$iter], ,$proc_sim_seconds[$iter]\n";
		print F3 "$cum_proc_sim_seconds,$proc_freq[3][$iter], ,$proc_int_util[3][$iter],$proc_float_util[3][$iter],$proc_mem_util[3][$iter], ,$proc_sim_seconds[$iter]\n";
		
		print F4 "$cum_proc_sim_seconds,$allcores_freq[$iter], ,$allcores_intutil[$iter],$allcores_floatutil[$iter],$allcores_memutil[$iter], ,$proc_sim_seconds[$iter]\n";
		
		#print F6 "$cum_proc_sim_seconds,$proc_dynp[0][$iter],$proc_dynp[1][$iter],$proc_dynp[2][$iter],$proc_dynp[3][$iter]\n";
		$iter++;
	}

        # print the %age time in each frequency stat
	open F37, ">$out_dir/core0PCtimefreq.csv" or die $!;
	print F37 "Freq - % time\n";
        foreach my $f (sort { $a <=> $b} keys %core0_PCtime_at_freq)
        {
            print F37 "$f, $core0_PCtime_at_freq{$f}\n";
        }
        close F37;
	open F37, ">$out_dir/core1PCtimefreq.csv" or die $!;
	print F37 "Freq - % time\n";
        foreach my $f (sort { $a <=> $b} keys %core1_PCtime_at_freq)
        {
            print F37 "$f, $core1_PCtime_at_freq{$f}\n";
        }
        close F37;
	open F37, ">$out_dir/core2PCtimefreq.csv" or die $!;
	print F37 "Freq - % time\n";
        foreach my $f (sort { $a <=> $b} keys %core2_PCtime_at_freq)
        {
            print F37 "$f, $core2_PCtime_at_freq{$f}\n";
        }
        close F37;
	open F37, ">$out_dir/core3PCtimefreq.csv" or die $!;
	print F37 "Freq - % time\n";
        foreach my $f (sort { $a <=> $b} keys %core3_PCtime_at_freq)
        {
            print F37 "$f, $core3_PCtime_at_freq{$f}\n";
        }
        close F37;


$STATS_FILE_I++;
print "##########################################################################################################################################\n";
}


#@S_BENCHMARKS = sort @BENCHMARKS;
#
#open EDP_FILE, ">$DATA_DIR/EDP.csv" or die $!;
#print EDP_FILE "benchmark,EDP\n";
#open EXEC_TIME_FILE, ">$DATA_DIR/EXEC_TIME.csv" or die $!;
#print EXEC_TIME_FILE "benchmark,exec_time\n";
#open ENERGY_FILE, ">$DATA_DIR/ENERGY.csv" or die $!;
#print ENERGY_FILE "benchmark,energy\n";
#
#my $bmark = 0;
#
#while ($bmark < @S_BENCHMARKS )
#{
#	print EDP_FILE "$S_BENCHMARKS[$bmark],$EDP_LIST{$S_BENCHMARKS[$bmark]}\n";
#	print EXEC_TIME_FILE "$S_BENCHMARKS[$bmark],$EXEC_TIME_LIST{$S_BENCHMARKS[$bmark]}\n";
#	print ENERGY_FILE "$S_BENCHMARKS[$bmark],$ENERGY_LIST{$S_BENCHMARKS[$bmark]}\n";
#	$bmark++
#}

# Useful commands

# $gpu_cycle_val = `grep gpu_sim_cycle $output_dir_var/log | awk '{ print \$NF }'`;

__END__



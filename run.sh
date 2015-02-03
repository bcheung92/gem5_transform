#!/bin/sh

export M5_PATH=/research/ljindal/gem5.oct18.new

/research/ljindal/gem5.oct18.new/build/ARM/gem5.fast --outdir=/users/ljindal/run /research/ljindal/gem5.oct18.new/configs/example/exp.3GHzfs.py -r 1 --checkpoint-dir=/research/ljindal/gem5.oct18.new/ondemand_ckpt --machine-type VExpress_EMM --kernel vmlinux.ondemand95.real --dtb-filename vexpress-v2p-ca15-tc1-gem5_dvfs_1cpus.dtb --cpu-type=DerivO3CPU --caches --disk-image=ARMv7a-ICS-Android.SMP.Asimbench-v3.img --script=/research/ljindal/gem5.oct18.new/asimbench/k9mail.rcS

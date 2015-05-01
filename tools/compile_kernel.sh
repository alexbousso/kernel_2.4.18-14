#!/bin/bash

KERNEL_NAME="2.4.18-14custom"
KERNEL_SRC_DIR="/usr/src/linux-${KERNEL_NAME}"


function runCommand {
    command=$1
    if [[ "${command}" == "" ]]; then
        echo "runCommand: Got an empty command!"
        return
    fi

    echo "runCommand: Running ${command}"
    ${command}
    retval=$?
    if [[ ${retval} -ne 0 ]]; then
        echo "********************************************************************************"
        echo "Error:"
        echo "runCommand: Command \"${command}\" failed and returned: ${retval}"
        echo "Aborting compilation"
        echo "********************************************************************************"
        # Exit script
        exit 1
    fi
}

# Copy modified files
FROM="/mnt/hgfs/kernel_2.4.18-14"
TO="/usr/src/linux-2.4.18-14custom"

runCommand "cp -vf ${FROM}/kernel/fork.c ${TO}/kernel"
runCommand "cp -vf ${FROM}/kernel/exit.c ${TO}/kernel"
runCommand "cp -vf ${FROM}/kernel/Makefile ${TO}/kernel"
runCommand "cp -vf ${FROM}/kernel/syscall_short.c ${TO}/kernel"
runCommand "cp -vf ${FROM}/arch/i386/kernel/entry.S ${TO}/arch/i386/kernel"
runCommand "cp -vf ${FROM}/include/linux/sched.h ${TO}/include/linux"
runCommand "cp -vf ${FROM}/kernel/sched.c ${TO}/kernel"

# Compile kernel
runCommand "cd ${KERNEL_SRC_DIR}"
runCommand "make bzImage"
#runCommand "make modules"
runCommand "make modules_install"

# Finished compiling - Copy bzImage to /boot and create initrd image
runCommand "cp ${KERNEL_SRC_DIR}/arch/i386/boot/bzImage /boot/vmlinuz-${KERNEL_NAME}"
runCommand "cd /boot"
runCommand "mv initrd-${KERNEL_NAME}.img initrd-${KERNEL_NAME}.img.bkup"
runCommand "mkinitrd initrd-${KERNEL_NAME}.img ${KERNEL_NAME}"

echo
echo "compile_kernel.sh: Done!"
echo

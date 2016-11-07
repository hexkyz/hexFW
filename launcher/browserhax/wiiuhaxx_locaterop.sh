ospath=$1
coreinit_textaddr=$2

powerpc-eabi-objcopy --change-section-address .text=$coreinit_textaddr $ospath/coreinit.elf $ospath/coreinit_reloc.elf

function getcoreinit_symboladdr
{
	val=`powerpc-eabi-readelf -a $ospath/coreinit_reloc.elf | grep "$1" | head -n 1 | cut -d: -f2 | cut "-d " -f2`
	echo "$2 = 0x$val;"
}

echo "<?php"
ropgadget_patternfinder $1/coreinit.elf --baseaddr=$coreinit_textaddr "--plainsuffix=;" --script=wiiuhaxx_locaterop_script
echo ""
getcoreinit_symboladdr "memcpy" "\$ROP_memcpy"
getcoreinit_symboladdr "DCFlushRange" "\$ROP_DCFlushRange"
getcoreinit_symboladdr "ICInvalidateRange" "\$ROP_ICInvalidateRange"
getcoreinit_symboladdr "OSSwitchSecCodeGenMode" "\$ROP_OSSwitchSecCodeGenMode"
getcoreinit_symboladdr "OSCodegenCopy" "\$ROP_OSCodegenCopy"
getcoreinit_symboladdr "OSGetCodegenVirtAddrRange" "\$ROP_OSGetCodegenVirtAddrRange"
getcoreinit_symboladdr "OSGetCoreId" "\$ROP_OSGetCoreId"
getcoreinit_symboladdr "OSGetCurrentThread" "\$ROP_OSGetCurrentThread"
getcoreinit_symboladdr "OSSetThreadAffinity" "\$ROP_OSSetThreadAffinity"
getcoreinit_symboladdr "OSYieldThread" "\$ROP_OSYieldThread"
getcoreinit_symboladdr "OSFatal" "\$ROP_OSFatal"
getcoreinit_symboladdr "_Exit" "\$ROP_Exit"
getcoreinit_symboladdr "OSScreenFlipBuffersEx" "\$ROP_OSScreenFlipBuffersEx"
getcoreinit_symboladdr "OSScreenClearBufferEx" "\$ROP_OSScreenClearBufferEx"
getcoreinit_symboladdr "OSDynLoad_Acquire" "\$ROP_OSDynLoad_Acquire"
getcoreinit_symboladdr "OSDynLoad_FindExport" "\$ROP_OSDynLoad_FindExport"
getcoreinit_symboladdr "__os_snprintf" "\$ROP_os_snprintf"
echo "?>"

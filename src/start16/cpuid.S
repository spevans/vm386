! converted from masm format to as86 format by simon evans 20-1-95
! output code changed from DOS routine to use custom routine
! code also changed to use external variables for cpu_type and fpu_type


!       Filename:       cpuid.S
!
!       This program has been developed by Intel Corporation.  You have
!       Intel's permission to incorporate this source code into your
!       product royalty free.
!
!       Intel specifically disclaims all warranties, express or implied,
!       and all liability, including consequential and other indirect
!       damages, for the use of this code, including liability for
!       infringement of any proprietary rights.  Intel does not assume
!       any responsibility for any errors which may appear in this code
!       nor any responsibility to update it.
!
!       This program contains three parts:
!       Part 1: Identifies CPU type in the variable cpu_type:
!               0=8086 processor
!               2=Intel 286 processor
!               3=Intel386(TM) processor
!               4=Intel486(TM) processor
!               5=Pentium(TM) processor
!
!       Part 2: Identifies FPU type in the variable fpu_type:
!               0=FPU not present
!               1=FPU present
!               2=287 present (only if cpu_type=3)
!               3=387 present (only if cpu_type=3)
!
!       Part 3: Prints out the appropriate message.  This part can
!               be removed if this program is not used in a DOS-based
!               system.  Portions affected are at the end of the
!               data segment and the print procedure in the code
!               segment.
!
!	REVISION HISTORY:
!	Date:  4/93
!	1. Replaced the .486 with .186 to avoid generation of 0FH type long
!	   conditional branches (such as the branch to end_get_cpuid at
!	   the end of the 8086/8088 test, near code address 003D).  These
!	   are not executable on the 8086/8088 and 80286, so the code falls
!	   into some strange place and hangs the system.  The .186 also allows
!	   multi-bit shifts to unpack CPUID info.  Using .186 requires 
!	   that all 32-bit operand prefix (66H) be inserted by hand.  
!	   This is done with a macro (OPND32).
!	2. Avoid all usage of 32-bit operands until it is clear that the
!	   CPU is at least an 80386.  The use of the Exx registers caused 
!	   the generation of the 66H prefix, which are not executed 
!	   correctly on the 8086/8088 and 80286.  
!	3. Eliminated all the register save/restore and added comment 
!	   that all registers are used by the functions.
!	4. Do the stack alignment just before messing with the AC bit in
!	   EFLAGS, otherwise the stack may not be properly aligned.  Also
!	   restore the AC bit immediately, so it does not stay set.
!	5. Changed the FPU detection to set one flag fpu_type (instead of
!	   the previous two flags: fpu_present and infinity).  An fpu_type
!	   of zero indicates no floating point unit is present, an fpu_type
!	   of 2 indicates an 80287 is present, an fpu_type of 3 indicates
!	   an 80387 is present.
!
!	If this code is assembled with MASM with no options specified, it
!	runs correctly on an 8086/8088, 80286, 80386, 80486, and 
!	Pentium(tm) processor.
!
!

!OPND32 MACRO op_code, op_erand
!        db      66h     ! Force 32-bit operand size
!  IFNB <op_code>
!        db      op_code
!    IFNB <op_erand>
!        dd      op_erand! 32-bit immediate value
!    ENDIF
!  ENDIF
!ENDM

!CPUID MACRO
!        db      0fh     ! Hardcoded opcode for CPUID instruction
!        db      0a2h
!ENDM

TRUE            =	1
FAMILY_MASK     =	0xf00
FAMILY_SHIFT    =	8
MODEL_MASK      =	0xf0
MODEL_SHIFT     =	4
STEPPING_MASK   =	0xf
FPU_FLAG        =	1
MCE_FLAG        =	0x80
CMPXCHG8B_FLAG  =	0x100

.text

.extern cpu_type, fpu_type, id_flag, vendor_id, model
.extern stepping, intel_proc, feature_flags

fp_status:	.word	0

!
! remove the remaining data declarations if not using the DOS-based
! print procedure
!
id_msg:		.ascii	"This system has a"
		.byte	0
fp_8087:	.ascii	" and an 8087 math coprocessor"
		.byte	0
fp_80287:	.ascii	" and an 80287 math coprocessor"
		.byte	0
fp_80387:	.ascii	" and an 80387 math coprocessor"
		.byte	0
c8086:		.ascii	"n 8086/8088 processor"
		.byte	0
c286:		.ascii	"n 80286 processor"
		.byte	0
c386:		.ascii	"n 80386 processor"
		.byte	0
c486:		.ascii	"n 80486 DX processor or 80487 SX math coprocessor"
		.byte	13,10,0,0
c486nfp:	.ascii	"n 80486 SX processor"
		.byte	0
Intel486_msg:	.byte	13,10
		.ascii	"This system contains a Genuine Intel486(TM) processor"
		.byte	13,10,0
Pentium_msg:	.byte	13,10
		.ascii	"This system contains a Genuine Intel Pentium(TM) processor"
		.byte	13,10,0
modelmsg:	.ascii	"Model:            "
		.byte	0
steppingmsg:	.ascii	"Stepping:         "
		.byte	0
familymsg:	.byte	13,10
		.ascii	"Processor Family: "
		.byte	0
period:		.ascii	"."
		.byte	13,10,0
dataCR:         .byte	0,13,10,0
intel_id:	.ascii	"GenuineIntel"
fpu_msg:	.byte	13,10
		.ascii	"This processor contains a FPU"
		.byte	13,10,0
mce_msg:	.ascii	"This processor supports the Machine Check Exception"
		.byte	13,10,0
cmp_msg:	.ascii	"This processor supports the CMPXCHG8B instruction"
		.byte	13,10,0
not_intel:	.ascii	"t least an 80486 processor."
		.byte	13,10,0
		.ascii	"It does not contain a Genuine Intel part and as a result,"
		.byte	13,10,0
		.ascii	"the CPUID detection information cannot be determined at this time."
		.byte	13,10,0

!
!       The purpose of this code is to identify the processor and
!       coprocessor that is currently in the system.  The program first
!       determines the processor id.  When that is accomplished,
!       the program then determines whether a coprocessor
!       exists in the system.  If a coprocessor or integrated
!       coprocessor exists, the program identifies
!       the coprocessor id.  The program then prints out
!       the CPU and floating point presence and type.
!

.extern PrintString
.globl cpuid

cpuid:  
        pushf                   ! save for restoration at end
        call    get_cpuid
        call    get_fpuid
        call    print
        popf
	ret


get_cpuid:
!
!       This procedure determines the type of CPU in a system
!       and sets the cpu_type variable with the appropriate
!       value.
!       All registers are used by this procedure, none are preserved.

!       Intel 8086 CPU check
!       Bits 12-15 of the FLAGS register are always set on the
!       8086 processor.
!
check_8086:
        pushf                   ! push original FLAGS
        pop     ax              ! get original FLAGS
        mov     cx, ax          ! save original FLAGS
        and     ax, #0x0fff	! clear bits 12-15 in FLAGS
        push    ax              ! save new FLAGS value on stack
        popf                    ! replace current FLAGS value
        pushf                   ! get new FLAGS
        pop     ax              ! store new FLAGS in AX
        and     ax, #0x0f000    ! if bits 12-15 are set, then CPU
        cmp     ax, #0x0f000    !   is an 8086/8088
        mov     cpu_type, #0    ! turn on 8086/8088 flag
        jne     check_80286   	! jump if CPU isnt 8086/8088
	ret

!       Intel 286 CPU check
!       Bits 12-15 of the FLAGS register are always clear on the
!       Intel 286 processor in real-address mode.
!
check_80286:
        or      cx, #0x0f000    ! try to set bits 12-15
        push    cx              ! save new FLAGS value on stack
        popf                    ! replace current FLAGS value
        pushf                   ! get new FLAGS
        pop     ax              ! store new FLAGS in AX
        and     ax, #0x0f000    ! if bits 12-15 clear, CPU=80286
        mov     cpu_type, #2    ! turn on 80286 flag
        jnz     check_80386     ! if bits set, CPU isnt 80286
	ret

!       Intel386 CPU check
!       The AC bit, bit #18, is a new bit introduced in the EFLAGS
!       register on the Intel486 DX CPU to generate alignment faults.
!       This bit cannot be set on the Intel386 CPU.
!
check_80386:
!       It is now safe to use 32-bit opcode/operands
        mov     bx, sp          ! save current stack pointer to align
        and     sp, #~3         ! align stack to avoid AC fault
        !OPND32
        pushfd                   ! push original EFLAGS
        !OPND32
        pop     eax              ! get original EFLAGS
        !OPND32
        mov     ecx, eax          ! save original EFLAGS
        !OPND32  35h, 40000h     ! flip AC bit in EFLAGS
	xor	eax,#0x40000
        !OPND32
        push    eax              ! save new EFLAGS value on stack
        !OPND32
        popfd                    ! replace current EFLAGS value
        !OPND32
        pushfd                  ! get new EFLAGS
        !OPND32
        pop     eax              ! store new EFLAGS in EAX
        !OPND32
        xor     eax,ecx          ! can't toggle AC bit, CPU=80386
        mov     cpu_type,#3     ! turn on 80386 CPU flag
        mov     sp, bx          ! restore original stack pointer
        jnz     not386          ! jump if 80386 CPU
	ret
not386:
        and     sp, #~3         ! align stack to avoid AC fault
        !OPND32
        push    ecx
        !OPND32
        popfd                    ! restore AC bit in EFLAGS first
        mov     sp, bx          ! restore original stack pointer

!       Intel486 DX CPU, Intel487 SX NDP, and Intel486 SX CPU check
!       Checking for ability to set/clear ID flag (Bit 21) in EFLAGS
!       which indicates the presence of a processor
!       with the ability to use the CPUID instruction.
!
check_80486:
        mov     cpu_type,#4     ! turn on 80486 CPU flag
        !OPND32
        mov     eax, ecx          ! get original EFLAGS
        !OPND32  35h, 200000h    ! flip ID bit in EFLAGS
	xor	eax, #0x200000
        !OPND32
        push    eax              ! save new EFLAGS value on stack
        !OPND32
        popfd                    ! replace current EFLAGS value
        !OPND32
        pushfd                   ! get new EFLAGS
        !OPND32
        pop     eax              ! store new EFLAGS in EAX
        !OPND32
        xor     eax, ecx          ! can't toggle ID bit,
        jne     check_vendor      !   CPU=80486
	ret

!       Execute CPUID instruction to determine vendor, family,
!       model and stepping.
!
check_vendor:
        mov     id_flag,#1              ! set flag indicating use of CPUID inst.
        !OPND32
        xor     eax, eax                  ! set up input for CPUID instruction
        !CPUID                           ! macro for CPUID instruction
	.byte	0x0f, 0xa2
        !OPND32
        mov     vendor_id,ebx           ! setup to test for vendor id
        !OPND32
        mov     vendor_id+4,edx
        !OPND32
        mov     vendor_id+8,ecx
        mov     si,#vendor_id
        mov     di,#intel_id
        mov     cx,#12
compare:
        repe
	cmpsb                   ! compare vendor id to "GenuineIntel"
        or      cx, cx
        jnz     end_get_cpuid           ! if not zero, not an Intel CPU,

intel_processor:
        mov     intel_proc,#1

cpuid_data:
        !OPND32
        cmp     eax,#1                   ! make sure 1 is a valid input
                                        ! value for CPUID
        jl      end_get_cpuid           ! if not, jump to end
        !OPND32
        xor     eax, eax                  ! otherwise, use as input to CPUID
        !OPND32
        inc     eax                      ! and get stepping, model and family
        !CPUID
	.byte	0x0f, 0xa2
        mov     stepping, al
        and     stepping, #STEPPING_MASK ! isolate stepping info

        and     al, #MODEL_MASK          ! isolate model info
        shr     al, #MODEL_SHIFT
        mov     model, al

        and     ax, #FAMILY_MASK         ! mask everything but family
        shr     ax, #FAMILY_SHIFT
        mov     cpu_type, al            ! set cpu_type with family

        !OPND32
        mov     feature_flags,edx       ! save feature flag data

end_get_cpuid:
        ret


!******************************************************************

get_fpuid:
!
!       This procedure determines the type of FPU in a system
!       and sets the fpu_type variable with the appropriate
!       value.
!       All registers are used by this procedure, none are preserved.

!       Coprocessor check
!       The algorithm is to determine whether the floating-point
!       status and control words can be written to.  If not, no
!       coprocessor exists.  If the status and control words can be
!       written to, the correct coprocessor is then determined
!       depending on the processor id.  The Intel386 CPU can
!       work with either an Intel287 NDP or an Intel387 NDP.
!       The infinity of the coprocessor must be
!       checked to determine the correct coprocessor id.

        fninit                  ! reset FP status word
        mov     fp_status,#0x5a5a ! initialize temp word to
                                ! non-zero value
        fnstsw  fp_status       ! save FP status word
        mov     ax, fp_status   ! check FP status word
        cmp     al, #0          ! see if correct status with
                                ! written
        mov     fpu_type, #0    ! no fpu present
        jne     end_get_fpuid

check_control_word:
        fnstcw  fp_status       ! save FP control word
        mov     ax, fp_status   ! check FP control word
        and     ax, #0x103f     ! see if selected parts
                                ! looks OK
        cmp     ax, #0x3f       ! check that 1's & 0's
                                ! correctly read
        mov     fpu_type,#0
        jne     end_get_fpuid
        mov     fpu_type,#1

!
!   80287/80387 check for the Intel386 CPU
!
check_infinity:
        cmpb    cpu_type,#3
        jne     end_get_fpuid
        fld1                    ! must use default control from FNINIT
        fldz                    ! form infinity
        fdivp	st(1), st       ! 8087 and Intel287 NDP say +inf = -inf
        fld     st              ! form negative infinity
        fchs                    ! Intel387 NDP says +inf <> -inf
        fcompp                  ! see if they are the same and remove them
        fstsw   fp_status       ! look at status from FCOMPP
        mov     ax, fp_status
        mov     fpu_type,#2     ! store Intel287 NDP for fpu type
        sahf                    ! see if infinities matched
        jz      end_get_fpuid   ! jump if 8087 or Intel287 is present
        mov     fpu_type,#3     ! store Intel387 NDP for fpu type
end_get_fpuid:
        ret


!*********************************************************************

print:
!
!       This procedure prints the appropriate cpuid string and
!       numeric processor presence status.  If the CPUID instruction
!       was supported, this procedure prints out cpuid info.
!       All registers are used by this procedure, none are preserved.

        cmpb    id_flag,#1              ! if set to 1, cpu supports
                                        !   CPUID instruction
                                        ! print detailed CPUID information
        je      print_cpuid_data

        mov     si,#id_msg              ! print initial message
	call	PrintString

print_86:
        cmpb    cpu_type,#0
        jne     print_286
        mov     si,#c8086
	call	PrintString
        cmpb    fpu_type,#0
        jne	got_fpu87
	ret
got_fpu87:
        mov     si,#fp_8087
	call	PrintString
	ret

print_286:
        cmpb    cpu_type,#2
        jne     print_386
        mov     si,#c286
	call	PrintString
        cmpb    fpu_type,#0
        jne    	got_fpu287 
	ret
got_fpu287:
        mov     si,#fp_80287
	call	PrintString
	ret

print_386:
        cmpb    cpu_type,#3
        jne     print_486
        mov     si,#c386
	call	PrintString
        cmpb    fpu_type,#0
        jne    	got_fpu2387 
	ret
got_fpu2387:
        cmpb    fpu_type,#2
        jne     print_387
        mov     si,#fp_80287
	call	PrintString
	ret

print_387:
        mov     si,#fp_80387
	call	PrintString
	ret

print_486:
        cmpb    fpu_type,#0
        je      print_Intel486sx
        mov     si,#c486
	call	PrintString
	ret

print_Intel486sx:
        mov     si,#c486nfp
	call	PrintString
	ret

print_cpuid_data:

cmp_vendor:
        cmpb    intel_proc,#1
	je	GenuineIntel
	mov	si,#not_intel
	call	PrintString
	ret
GenuineIntel:
        cmpb    cpu_type,#4                     ! if cpu_type=4, print
                                                ! Intel486 CPU message
        jne     check_Pentium
        mov     si,#Intel486_msg
	call	PrintString
        jmp     print_family

check_Pentium:
        cmpb    cpu_type,#5                     ! if cpu_type=5, print
        jne     print_features                  ! Pentium processor message
        mov     si,#Pentium_msg
	call	PrintString

print_family:
        mov     si,#familymsg            ! print family msg
	call	PrintString
        mov     al, cpu_type
        mov     dataCR, al
        add     dataCR,#0x30            ! convert to ASCII
        mov    	si,#dataCR               ! print family info
	call	PrintString

print_model:
        mov     si,#modelmsg             ! print model msg
	call	PrintString
        mov     al, model
        mov     dataCR, al
        add     dataCR,#0x30            ! convert to ASCII
        mov     si,#dataCR               ! print model info
	call	PrintString

print_stepping:
        mov     si,#steppingmsg          ! print stepping msg
	call	PrintString
        mov     al, stepping
        mov     dataCR, al
        add     dataCR,#0x30            ! convert to ASCII
        mov     si,#dataCR               ! print stepping info
	call	PrintString

print_features:
        mov     ax, feature_flags
        and     ax, #FPU_FLAG                    ! check for FPU
        jz      check_MCE
        mov     si,#fpu_msg
	call	PrintString

check_MCE:
        mov     ax, feature_flags
        and     ax, #MCE_FLAG                    ! check for MCE
        jz      check_CMPXCHG8B
        mov     si,#mce_msg
	call	PrintString

check_CMPXCHG8B:
        mov     ax, feature_flags
        and     ax, #CMPXCHG8B_FLAG              ! check for CMPXCHG8B
        jz      end_print
        mov     si,#cmp_msg
	call	PrintString
end_print:
        ret

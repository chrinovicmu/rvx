#include "RVXMCCodeEmitter.h"
#include "RVXFixupKinds.h"       // Our RVX::fixup_rvx_xxx enum values
#include "RVXMCTargetDesc.h"     // Register enum (RVX::X0 etc.)

#include "llvm/MC/MCContext.h"         
#include "llvm/MC/MCExpr.h"            
#include "llvm/MC/MCFixup.h"           
#include "llvm/MC/MCInst.h"            
#include "llvm/MC/MCInstrInfo.h"       
#include "llvm/MC/MCRegisterInfo.h" 
#include "llvm/MC/MCSubtargetInfo.h"   
#include "llvm/Support/Casting.h"      
#include "llvm/Support/EndianStream.h" 
#include "llvm/Support/raw_ostream.h"  
#include <cassert>
#include <cstdint>
#include <llvm-14/llvm/ADT/SmallVector.h>
#include <llvm-14/llvm/ADT/Twine.h>
#include <llvm-14/llvm/MC/MCInstPrinter.h>
#include <llvm-14/llvm/MC/MCInstrDesc.h>

using namespace llvm; 

#define GET_INSTRINFO_MC_DESC 
#include "RVXGenMCCodeEmitter.inc"

RVXMCCodeEmitter::RVXMCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx)
    : MCII(MCII), Ctx(Ctx); 


void RVXMCCodeEmitter::encodeInstruction(const MCInst &Inst, 
                                         raw_ostream &OS, 
                                         SmallVectorImpl<MCFixup> &Fixups, 
                                         const MCSubtargetInfo &STI) const{
    
    const MCInstrDesc &Desc = MCII.get(Inst.getOpcode()); 
    usigned Size = Desc.getSize(); 

    /*RV32 for now */ 
    if(Size != 4){
        Ctx.reportError(Inst.getLoc(),
                        "RVXMCCodeEmitter: unrecoginzed isntruction size " *
                        Twine(Size) + "bytes for opcode " + 
                        Twine(Inst.getOpcode()); 
        return; 
    }

    uint64_t Binary = getBinaryCodeForInstr(Inst, Fixups, STI); 

    if(Size == 4)
    {
        support::endian::write<uint32_t>(OS, static_casr<uint32_t>(Binary), 
                                         llvm::endianness::little); 
    }
}

uint64_t RVXMCCodeEmitter::getMachineOpValue(const MCInst &MI ,
                                             const MCOperand &MO, 
                                             SmallVectorImpl<MCFixup> &Fixups, 
                                             const MCSubtargetInfo &STI) const{

    if(MO.isReg()){
        return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg()); 
    }

    if(MO.isImm()){
        return static_cast<uint64_t>(MO.getImm()); 
    }

    llvm_unreachable("getMachineOpValue called on expression operand — "
                   "add EncoderMethod to the operand's .td definition");
}

uint64_t RVXMCCodeEmitter::getImmOpValue(const MCInst &MI, unsigned OpNo,
                                         SmallVectorImpl<MCFixups> &Fixups, 
                                         const MCSubtargetInfo &STI) const{
    const MCOperand &MO = MI.getOperand(OpNo); 

    if(MO.isImm()){
        return MO.getImm(); 
    }

    /*expression path, create fixeup and return 0 as palceholder*/ 
    assert(MO.isExpr() && "Epected register, immedaite, or expression"); 
    const MCExpr *Expr = MO.getExpr(); 

    MCFixupKind Kind = MCFixupKind("RVX::fixup_rvx_32"); 
    Fixups.push_back(MCFixup::create(0, Expr, Kind, MI.getLoc())); 

    return 0; 
}

uint64_t RVXMCCodeEmitter::getImmOpValueSExt12(const MCInst, unsigned OpNo, 
                                               SmallVectorImpl<MCFixup> &Fixups, 
                                               const MCSubtargetInfo &STI) const {

    const MCOperand &MO = MI.getOperand(OpNo); 

    if(M0.isImm()){
        int64_t Value = MO.getImm(); 

        /*if value fits in signed bit 12-bit field
        * signed 12 bit range = -2048, 2047*/ 
        if (Value < -2048 || Value > 2047) {
            Ctx.reportError(MI.getLoc(),
                      "immediate operand " + Twine(Value) +
                      " out of range for 12-bit signed field [-2048, 2047]");
        return 0;
        }

        return static_cast<uint64_t>(Value) & 0xFFF; 
    }

    assert(MO.isExpr());
    MCFixupKind Kind = MCFixupKind(RVX::fixup_rvx_lo12_i);
    Fixups.push_back(MCFixup::create(0, MO.getExpr(), Kind, MI.getLoc()));
    return 0;
}

uint64_t RVXMCCodeEmitter::getImmOpValueAsr1(const MCInst &MI, unsigned OpNo,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  
    const MCOperand &MO = MI.getOperand(OpNo);

    if (MO.isImm()) {
        int64_t Value = MO.getImm();

        // Branch offsets must be even (2-byte aligned). An odd offset means
        // the target is in the middle of an instruction — illegal on RISC-V.
        if (Value & 1) {
        Ctx.reportError(MI.getLoc(),
                      "branch target offset " + Twine(Value) +
                      " is not 2-byte aligned");
        return 0;
        }

        // The 13-bit signed range for branch offsets is [-4096, 4094].
        // (13 bits: -(2^12) to (2^12 - 2) with bit 0 always 0.)
        if (Value < -4096 || Value > 4094) {
        Ctx.reportError(MI.getLoc(),
                      "branch offset " + Twine(Value) +
                      " out of range [-4096, 4094]");
        return 0;
        }

        // Shift right by 1: the hardware will shift left by 1 during execution,
        // recovering the original byte offset. We store only bits [12:1].
        return (static_cast<uint64_t>(Value) >> 1) & 0xFFF;
    }

     assert(MO.isExpr());
    MCFixupKind Kind = MCFixupKind(RVX::fixup_rvx_branch_pcrel_12);
    Fixups.push_back(MCFixup::create(0, MO.getExpr(), Kind, MI.getLoc()));
    return 0;
}

uint64_t RVXMCCodeEmitter::getImmOpValueHi20(const MCInst &MI, unsigned OpNo,
                                        SmallVectorImpl<MCFixup> &Fixups,
                                        const MCSubtargetInfo &STI) const {

    const MCOperand &MO = MI.getOperand(OpNo);

    if (MO.isImm()) {
        // For a constant immediate, validate that it fits in 20 bits.
        // The unsigned 20-bit range is [0, 0xFFFFF] = [0, 1048575].
        int64_t Value = MO.getImm();
        if (Value < 0 || Value > 0xFFFFF) {
        Ctx.reportError(MI.getLoc(),
                      "upper immediate " + Twine(Value) +
                      " out of range for 20-bit unsigned field [0, 0xFFFFF]");
         return 0;
        }

        // Return bits [19:0]; TableGen places them in instruction bits [31:12].
        return static_cast<uint64_t>(Value) & 0xFFFFF;
    }

    assert(MO.isExpr());
    MCFixupKind Kind = MCFixupKind(RVX::fixup_rvx_hi20);
    Fixups.push_back(MCFixup::create(0, MO.getExpr(), Kind, MI.getLoc()));
    return 0;
}





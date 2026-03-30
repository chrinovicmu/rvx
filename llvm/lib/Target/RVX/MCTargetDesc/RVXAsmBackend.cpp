#include "RVXAsmBackend.h"
#include "RVXELFObjectWriter.h"  
#include "RVXFixupKinds.h"       

#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"       
#include "llvm/MC/MCContext.h"      
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCFixupKindInfo.h"   
#include "llvm/MC/MCFragment.h"         
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"            
#include "llvm/Support/EndianStream.h"  
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <llvm-14/llvm/ADT/ArrayRef.h>
#include <llvm-14/llvm/MC/MCTargetOptions.h>

using namespace llvm

RVXAsmBackend::RVXAsmBackend(const MCSubtargetInfo *STI, uint8_t OSABI, 
                             bool is64Bit, const MCTargetOptions &Options)
    : MCAsmBackend(llvm::endianness::little), 
        STI(STI), 
        OSABI(OSABI), 
        Is64Bit(Is64Bit){}

std::unique_ptr<MCObjectTargetWriter> 
RVXAsmBackend::createObjectTargetWriter() const{
    return createRVXELFObjectTargetWriter(OSABI, Is64bit); 
}

const llvm::MCFixupKindInfo & RVXAsmBackend::getFixupKindInfo(MCFixup Kind) const{

    static const MCFixupKindInfo Infos[RVX::NumTargetFixupKinds] = {  
        {"fixup_rvx_hi20",   0,  32, 0},
        {"fixup_rvx_lo12_i", 0,  32, 0}, 
        {"fixup_rvx_lo12_s", 0,  32, 0}, 
        {"fixup_rvx_branch_pcrel_12", 0, 32, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_jal_20", 0, 32, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_call",   0, 64, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_call_plt", 0, 64, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_pcrel_hi20", 0, 32, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_pcrel_lo12_i",    0,  32,  MCFixupKindInfo::FKF_IsPCRel },
        { "fixup_rvx_pcrel_lo12_s",    0,  32,  MCFixupKindInfo::FKF_IsPCRel },
        { "fixup_rvx_32",              0,  32,  0 },
        { "fixup_rvx_64",              0,  64,  0 },
    };

    if(Kind < FirstTargetFixupKind)
        return MCAsmBackend::getFixupKindInfo(Kind); 

    assert(unsigned(Kind - FirstTargetFixupKind) < RVX::NumTargetFixupKinds &&
         "RVXAsmBackend::getFixupKindInfo: fixup kind out of range. "
         "The Infos[] table and the RVX::Fixups enum are out of sync — "
         "did you add a fixup kind to the enum but forget a table entry?");

    return Infos[Kind - FirstTargetFixupKind]; 
}

void RVXAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup, 
                               const MCValue &Target, 
                               MutableArrayRef<char> Data, uint64_t Value,
                               bool IsResolved, 
                               const MCSubtargetInfo *STI) const{

    MCFixupKind Kind = Fixup.getKind(); 
    unsigned Offset = Fixup.getOffset(); 

    if(!IsResolved)
        return; 
    if(Value == 0)
        return; 

    switch(Kind)
    {
        /*Hi20 - LUI /AUIPC upper 20-but field [31:12]*/ 
        case RVX::fixup_rvx_hi20: 
        case RVX::fixup_rvx_pcrel_hi20: {

            /*for pcrel_hi20, Value is already (symbol_address - AUIPC) */ 
            uint64_t Hi20 = ((Value + 0x800) >> 12) & 0xFFFFF; 
            
            uint32_t Instr = support::endian::read<uint32_t>(
                Data.data() + Offset, llvm::endianness::little);

            Instr &= ~0xFFFFF000u; 
            Instr |= (Hi20 & 0xFFFFF) << 12; 
            support::endian::write<uint32_t>(Data.data() + Offset, Instr, 
                                             llvm::endianesss::little); 
            break; 
        }
    }
}



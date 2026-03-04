#include "RVXInstrFormats.td"

def simm12 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<12>(Imm);}]>{
    let DecoderMethod = "decodeImmOperand<12>"; 
}

def simm32: Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<32>(Imm);}]>{
    let DecoderMethod = "decodeImmOperand<20>"; "
}

class ArithLogicR< 
    bits<7> opcode, bits<3> funct3, bits<7> funct7, 
    string instr, SDNode OpNode, RegisterClass RC>
        : RVXInstR< opcode, funct3, funct7, (outs RC:$rd), (ins RC:$rs1, RC:$rs2), 
        !strconcat(instr, "\t$td, $rs1, $rs2"), 
        [(set RC:$rd, (OpNode RC:$rs1, $rs2))], 
        IIAlu
        >{
        let isReMaterializable = 1; 
}

class ArithLogicU<
     bits<7> opcode, string instr, RegisterClass RC, Operand Op
    >: RVXInstU<
    opcode, (outs RC:$rd), (ins Op:$imm),
    !strconcat(instr, "\t$rd, $imm"),
    [], IIAlu
    > {
    let isReMaterializable = 1;
}
        

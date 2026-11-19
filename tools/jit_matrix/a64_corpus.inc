// Corpus of arm64_emit.cpp primitive calls.
//
// Included inside main() by both dump tools: once against the hand-written
// encoder, once against the asmjit one. Every primitive gets at least one call
// and every branch inside a primitive gets its own case, because the whole
// point is to catch a translation that changed an encoding.
//
// Argument conventions follow the primitives themselves: (rs1 = Rn, rs2 = Rm,
// rt = Rd) for the ALU ones, (rs = base, rt = target) for the loads/stores.

// ---------------------------------------------------------------- shifts, imm
A64_CASE("lsl r3, #0",        emit_lsl(g_out, 3, 0));
A64_CASE("lsl r3, #31",       emit_lsl(g_out, 3, 31));
A64_CASE("lsr r3, #0",        emit_lsr(g_out, 3, 0));
A64_CASE("lsr r3, #31",       emit_lsr(g_out, 3, 31));
A64_CASE("lsr r3, #32",       emit_lsr(g_out, 3, 32));
A64_CASE("lsr r3, #40",       emit_lsr(g_out, 3, 40));
A64_CASE("lsr64 x3, #8",      emit_lsr64(g_out, 3, 8));
A64_CASE("lsr64 x3, #40",     emit_lsr64(g_out, 3, 40));
A64_CASE("asr r3, #0",        emit_asr(g_out, 3, 0));
A64_CASE("asr r3, #31",       emit_asr(g_out, 3, 31));
A64_CASE("asr r3, #32",       emit_asr(g_out, 3, 32));
A64_CASE("asr64 x3, #8",      emit_asr64(g_out, 3, 8));
A64_CASE("asr64 x3, #40",     emit_asr64(g_out, 3, 40));
A64_CASE("ror r3, #0",        emit_ror(g_out, 3, 0));
A64_CASE("ror r3, #31",       emit_ror(g_out, 3, 31));
A64_CASE("ror r3, #32",       emit_ror(g_out, 3, 32));
A64_CASE("rrx r3, #1",        emit_rrx(g_out, 3, 1));
A64_CASE("lsls r3, #0",       emit_lsls(g_out, 3, 0));
A64_CASE("lsrs r3, #7",       emit_lsrs(g_out, 3, 7));
A64_CASE("asrs r3, #7",       emit_asrs(g_out, 3, 7));
A64_CASE("rors r3, #7",       emit_rors(g_out, 3, 7));
A64_CASE("rrxs r3, #7",       emit_rrxs(g_out, 3, 7));

// ------------------------------------------------------------ shifts, register
A64_CASE("lsl_reg r3, r4",       emit_lsl_reg(g_out, 3, 4));
A64_CASE("lsl_reg_quick r3, r4", emit_lsl_reg_quick(g_out, 3, 4));
A64_CASE("lsr_reg_quick r3, r4", emit_lsr_reg_quick(g_out, 3, 4));
A64_CASE("lsr_reg r3, r4",       emit_lsr_reg(g_out, 3, 4));
A64_CASE("asr_reg r3, r4",       emit_asr_reg(g_out, 3, 4));
A64_CASE("ror_reg r3, r4",       emit_ror_reg(g_out, 3, 4));
A64_CASE("lsls_reg r3, r4",      emit_lsls_reg(g_out, 3, 4));
A64_CASE("lsrs_reg r3, r4",      emit_lsrs_reg(g_out, 3, 4));
A64_CASE("asrs_reg r3, r4",      emit_asrs_reg(g_out, 3, 4));
A64_CASE("rors_reg r3, r4",      emit_rors_reg(g_out, 3, 4));
A64_CASE("rrxs_reg r3, r4",      emit_rrxs_reg(g_out, 3, 4));

// --------------------------------------------------------------- data procs
A64_CASE("mov r0, r1",     emit_mov(g_out, 1, 0));
A64_CASE("mov64 x0, x1",   emit_mov64(g_out, 1, 0));
A64_CASE("add r2, r3, r4", emit_add(g_out, 3, 4, 2));
A64_CASE("adds r2, r3, r4", emit_adds(g_out, 3, 4, 2));
A64_CASE("add64 x2, x3, x4", emit_add64(g_out, 3, 4, 2));
A64_CASE("adc r2, r3, r4", emit_adc(g_out, 3, 4, 2));
A64_CASE("adcs r2, r3, r4", emit_adcs(g_out, 3, 4, 2));
A64_CASE("sub r2, r3, r4", emit_sub(g_out, 3, 4, 2));
A64_CASE("subs r2, r3, r4", emit_subs(g_out, 3, 4, 2));
A64_CASE("sbc r2, r3, r4", emit_sbc(g_out, 3, 4, 2));
A64_CASE("sbcs r2, r3, r4", emit_sbcs(g_out, 3, 4, 2));
A64_CASE("neg r2, r3",    emit_neg(g_out, 3, 2));
A64_CASE("negs r2, r3",   emit_negs(g_out, 3, 2));
A64_CASE("mvns r2, r3",   emit_mvns(g_out, 3, 2));
A64_CASE("zeroreg r2",    emit_zeroreg(g_out, 2));
A64_CASE("movs r2, r3",   emit_movs(g_out, 3, 2));
A64_CASE("mov_cond r2, r3, eq", emit_mov_cond(g_out, 3, 2, 0x0));
A64_CASE("mov_cond r2, r3, al", emit_mov_cond(g_out, 3, 2, 0xE));
A64_CASE("test r2, r3",   emit_test(g_out, 2, 3));
A64_CASE("not r2, r3",    emit_not(g_out, 3, 2));
A64_CASE("and r2, r3, r4", emit_and(g_out, 3, 4, 2));
A64_CASE("or r2, r3, r4",  emit_or(g_out, 3, 4, 2));
A64_CASE("or64 x2, x3, x4", emit_or64(g_out, 3, 4, 2));
A64_CASE("or_and_set_flags r2, r3, r4", emit_or_and_set_flags(g_out, 3, 4, 2));
A64_CASE("xor r2, r3, r4", emit_xor(g_out, 3, 4, 2));
A64_CASE("ands r2, r3, r4", emit_ands(g_out, 3, 4, 2));
A64_CASE("ors r2, r3, r4",  emit_ors(g_out, 3, 4, 2));
A64_CASE("xors r2, r3, r4", emit_xors(g_out, 3, 4, 2));
A64_CASE("cmp r2, r3",      emit_cmp(g_out, 2, 3));
A64_CASE("cmpx64 x2",       emit_cmpx64(g_out, 2));
A64_CASE("clz r2, r3",      emit_clz(g_out, 3, 2));
A64_CASE("teq r2, r3",      emit_teq(g_out, 2, 3));

// ------------------------------------------------------------ move immediates
A64_CASE("movz r2, #0",        emit_movz(g_out, 0, 2));
A64_CASE("movz r2, #0xFFFF",   emit_movz(g_out, 0xFFFF, 2));
A64_CASE("movz_lsl16 r2, #1",  emit_movz_lsl16(g_out, 1, 2));
A64_CASE("movn r2, #0",        emit_movn(g_out, 0, 2));
A64_CASE("movn_lsl16 r2, #1",  emit_movn_lsl16(g_out, 1, 2));
A64_CASE("movk r2, #0x1234",   emit_movk(g_out, 0x1234, 2));
A64_CASE("movk_lsl16 r2, #0x1234", emit_movk_lsl16(g_out, 0x1234, 2));
A64_CASE("movk_lslnum r2, #0x1234, 0",  emit_movk_lslnum(g_out, 0x1234, 2, 0));
A64_CASE("movk_lslnum r2, #0x1234, 16", emit_movk_lslnum(g_out, 0x1234, 2, 16));
A64_CASE("movk_lslnum64 x2, #0x1234, 16", emit_movk_lslnum64(g_out, 0x1234, 2, 16));
A64_CASE("movk_lslnum64 x2, #0x1234, 32", emit_movk_lslnum64(g_out, 0x1234, 2, 32));
A64_CASE("movk_lslnum64 x2, #0x1234, 48", emit_movk_lslnum64(g_out, 0x1234, 2, 48));
A64_CASE("movz64 x2, #0x1234", emit_movz64(g_out, 0x1234, 2));
A64_CASE("movimms r2, #0x1234", emit_movimms(g_out, 0x1234, 2));

A64_CASE("ptr 0",                  emit_ptr(g_out, 0, 2));
A64_CASE("ptr 0x1234",             emit_ptr(g_out, 0x1234, 2));
A64_CASE("ptr 0x12340000",         emit_ptr(g_out, 0x12340000u, 2));
A64_CASE("ptr 0x12345678",         emit_ptr(g_out, 0x12345678u, 2));
A64_CASE("ptr 0x123456789ABCDEF0", emit_ptr(g_out, 0x123456789ABCDEF0ull, 2));
A64_CASE("ptr 0xFFFFFFFF",         emit_ptr(g_out, 0xFFFFFFFFu, 2));

// Every branch of emit_movimm's expansion gets one value.
A64_CASE("movimm 0",          emit_movimm(g_out, 0, 2));            // movz
A64_CASE("movimm 0xFFFF",     emit_movimm(g_out, 0xFFFF, 2));       // movz
A64_CASE("movimm 0x10000",    emit_movimm(g_out, 0x10000, 2));      // movz lsl 16
A64_CASE("movimm 0xFFFF0000", emit_movimm(g_out, 0xFFFF0000u, 2));  // movn lsl 16
A64_CASE("movimm 0xFFFFFFFE", emit_movimm(g_out, 0xFFFFFFFEu, 2));  // movn
A64_CASE("movimm 0x00FF00FF", emit_movimm(g_out, 0x00FF00FFu, 2));  // genimm2 -> orr
A64_CASE("movimm 0x12345678", emit_movimm(g_out, 0x12345678u, 2));  // movz lsl 16 + movk
A64_CASE("movimm 0x1234567",  emit_movimm(g_out, 0x1234567u, 2));   // movz lsl 16 + movk

// ---------------------------------------------------------------- add/cmp imm
A64_CASE("addimm 0",        emit_addimm(g_out, 3, 0, 2));
A64_CASE("addimm 0 same",   emit_addimm(g_out, 3, 0, 3));
A64_CASE("addimm 1",        emit_addimm(g_out, 3, 1, 2));
A64_CASE("addimm 4095",     emit_addimm(g_out, 3, 4095, 2));
A64_CASE("addimm 4096",     emit_addimm(g_out, 3, 4096, 2));
A64_CASE("addimm -1",       emit_addimm(g_out, 3, -1, 2));
A64_CASE("addimm -4095",    emit_addimm(g_out, 3, -4095, 2));
A64_CASE("addimm -4096",    emit_addimm(g_out, 3, -4096, 2));
A64_CASE("addimm -4097",    emit_addimm(g_out, 3, -4097, 2));
A64_CASE("addimm 0x123456", emit_addimm(g_out, 3, 0x123456, 2));
A64_CASE("addimm64 0x1234", emit_addimm64(g_out, 3, 0x1234, 2));
A64_CASE("addsimm 0x1234",  emit_addsimm(g_out, 3, 0x1234, 2));
A64_CASE("subimm 0x1234",   emit_subimm(g_out, 3, 0x1234, 2));
A64_CASE("subsimm 0x1234",  emit_subsimm(g_out, 3, 0x1234, 2));
A64_CASE("cmpimm 0",        emit_cmpimm(g_out, 3, 0));
A64_CASE("cmpimm 0x1234",   emit_cmpimm(g_out, 3, 0x1234));
A64_CASE("testimm 0xFF00",  emit_testimm(g_out, 3, 0xFF00));
A64_CASE("testimm64 0xFF00", emit_testimm64(g_out, 3, 0xFF00));
A64_CASE("orimm 0",         emit_orimm(g_out, 3, 0, 2));
A64_CASE("orimm 0 same",    emit_orimm(g_out, 3, 0, 3));
A64_CASE("orimm 0xFF00",    emit_orimm(g_out, 3, 0xFF00, 2));
A64_CASE("xorimm 0x1234",   emit_xorimm(g_out, 3, 0x1234, 2));
A64_CASE("andimm 0",        emit_andimm(g_out, 3, 0, 2));
A64_CASE("andimm 0xFF00",   emit_andimm(g_out, 3, 0xFF00, 2));
A64_CASE("andimm 0x12345",  emit_andimm(g_out, 3, 0x12345, 2));
A64_CASE("andsimm 0",       emit_andsimm(g_out, 3, 0, 2));
A64_CASE("andsimm 0x1F",    emit_andsimm(g_out, 3, 0x1F, 2));
A64_CASE("andsimm 0x12345", emit_andsimm(g_out, 3, 0x12345, 2));

// -------------------------------------------------------------- multiplication
A64_CASE("smulbb r2, r3", emit_smulbb(g_out, 2, 3));
A64_CASE("smulbt r2, r3", emit_smulbt(g_out, 2, 3));
A64_CASE("smultb r2, r3", emit_smultb(g_out, 2, 3));
A64_CASE("smultt r2, r3", emit_smultt(g_out, 2, 3));
A64_CASE("smulwt r2, r3", emit_smulwt(g_out, 2, 3));
A64_CASE("smulwb r2, r3", emit_smulwb(g_out, 2, 3));
A64_CASE("smlawb r0, r1, r2, r3", emit_smlawb(g_out, 0, 1, 2, 3));
A64_CASE("smlawt r0, r1, r2, r3", emit_smlawt(g_out, 0, 1, 2, 3));
A64_CASE("smlabb r0, r1, r2, r3", emit_smlabb(g_out, 0, 1, 2, 3));
A64_CASE("smlabt r0, r1, r2, r3", emit_smlabt(g_out, 0, 1, 2, 3));
A64_CASE("smlatb r0, r1, r2, r3", emit_smlatb(g_out, 0, 1, 2, 3));
A64_CASE("smlatt r0, r1, r2, r3", emit_smlatt(g_out, 0, 1, 2, 3));
A64_CASE("smlalbb r0, r1, r2, r3", emit_smlalbb(g_out, 0, 1, 2, 3));
A64_CASE("smlalbt r0, r1, r2, r3", emit_smlalbt(g_out, 0, 1, 2, 3));
A64_CASE("smlaltb r0, r1, r2, r3", emit_smlaltb(g_out, 0, 1, 2, 3));
A64_CASE("smlaltt r0, r1, r2, r3", emit_smlaltt(g_out, 0, 1, 2, 3));
A64_CASE("mul r2, r3",  emit_mul(g_out, 2, 3));
A64_CASE("muls r2, r3", emit_muls(g_out, 2, 3));
A64_CASE("mla r0, r1, r2, r3",  emit_mla(g_out, 0, 1, 2, 3));
A64_CASE("mlas r0, r1, r2, r3", emit_mlas(g_out, 0, 1, 2, 3));
A64_CASE("umull r0, r1, r2, r3",  emit_umull(g_out, 0, 1, 2, 3));
A64_CASE("umulls r0, r1, r2, r3", emit_umulls(g_out, 0, 1, 2, 3));
A64_CASE("umlal r0, r1, r2, r3",  emit_umlal(g_out, 0, 1, 2, 3));
A64_CASE("umlals r0, r1, r2, r3", emit_umlals(g_out, 0, 1, 2, 3));
A64_CASE("smull r0, r1, r2, r3",  emit_smull(g_out, 0, 1, 2, 3));
A64_CASE("smulls r0, r1, r2, r3", emit_smulls(g_out, 0, 1, 2, 3));
A64_CASE("smlal r0, r1, r2, r3",  emit_smlal(g_out, 0, 1, 2, 3));
A64_CASE("smlals r0, r1, r2, r3", emit_smlals(g_out, 0, 1, 2, 3));

// ------------------------------------------------------------- load and store
A64_CASE("ldr w2, [x3]",   emit_ldr(g_out, 3, 2));
A64_CASE("ldr64 x2, [x3]", emit_ldr64(g_out, 3, 2));
A64_CASE("str w2, [x3]",   emit_str(g_out, 3, 2));
A64_CASE("str64 x2, [x3]", emit_str64(g_out, 3, 2));
A64_CASE("str_cond w2, [x3], eq", emit_str_cond(g_out, 3, 2, 0x0));
A64_CASE("str_cond w2, [x3], al", emit_str_cond(g_out, 3, 2, 0xE));
A64_CASE("ldrb w2, [x3]",  emit_ldrb(g_out, 3, 2));
A64_CASE("ldrsb w2, [x3]", emit_ldrsb(g_out, 3, 2));
A64_CASE("strb w2, [x3]",  emit_strb(g_out, 3, 2));
A64_CASE("ldrsw w2, [x3]", emit_ldrsw(g_out, 3, 2));
A64_CASE("ldrs w2, [x3]",  emit_ldrs(g_out, 3, 2));
A64_CASE("ldrw w2, [x3]",  emit_ldrw(g_out, 3, 2));
A64_CASE("strw w2, [x3]",  emit_strw(g_out, 3, 2));

A64_CASE("str_ptr_cond",   emit_str_ptr_cond(g_out, 0x12345678, 2, 0x1));
A64_CASE("write_ptr32",    emit_write_ptr32(g_out, 0x12345678, 0xABCD));
A64_CASE("write_ptr",      emit_write_ptr(g_out, 0x12345678, 0x1234567890ull));

A64_CASE("write_ptr32_regptrTO_immFROM",       emit_write_ptr32_regptrTO_immFROM(g_out, 3, 0x1234));
A64_CASE("write_ptr32_regptrTO_regFROM",       emit_write_ptr32_regptrTO_regFROM(g_out, 3, 4));
A64_CASE("write_ptr32_regptrTO_regptrFROM",    emit_write_ptr32_regptrTO_regptrFROM(g_out, 3, 4));
A64_CASE("read_ptr32_regptrFROM_regTO",        emit_read_ptr32_regptrFROM_regTO(g_out, 3, 4));
A64_CASE("write_ptr32_reg",                    emit_write_ptr32_reg(g_out, 0x12345678, 4));
A64_CASE("writeBYTE_ptr32_regptrTO_immFROM",   emit_writeBYTE_ptr32_regptrTO_immFROM(g_out, 3, 0x12));
A64_CASE("writeBYTE_ptr32_regptrTO_regFROM",   emit_writeBYTE_ptr32_regptrTO_regFROM(g_out, 3, 4));
A64_CASE("writeBYTE_ptr32_regptrTO_regptrFROM", emit_writeBYTE_ptr32_regptrTO_regptrFROM(g_out, 3, 4));
A64_CASE("readBYTE_ptr32_regptrFROM_regTO",    emit_readBYTE_ptr32_regptrFROM_regTO(g_out, 3, 4));
A64_CASE("writeWORD_ptr32_regptrTO_immFROM",   emit_writeWORD_ptr32_regptrTO_immFROM(g_out, 3, 0x1234));
A64_CASE("writeWORD_ptr32_regptrTO_regFROM",   emit_writeWORD_ptr32_regptrTO_regFROM(g_out, 3, 4));
A64_CASE("writeWORD_ptr32_regptrTO_regptrFROM", emit_writeWORD_ptr32_regptrTO_regptrFROM(g_out, 3, 4));
A64_CASE("readWORD_ptr32_regptrFROM_regTO",    emit_readWORD_ptr32_regptrFROM_regTO(g_out, 3, 4));

A64_CASE("add_ptr32_regptrTO_regFROM",    emit_add_ptr32_regptrTO_regFROM(g_out, 3, 4));
A64_CASE("adds_ptr32_regptrTO_regFROM",   emit_adds_ptr32_regptrTO_regFROM(g_out, 3, 4));
A64_CASE("add_ptr32_regptrTO_immFROM",    emit_add_ptr32_regptrTO_immFROM(g_out, 3, 0x1234));
A64_CASE("adds_ptr32_regptrTO_immFROM",   emit_adds_ptr32_regptrTO_immFROM(g_out, 3, 0x1234));
A64_CASE("sub_ptr32_regptrTO_regFROM",    emit_sub_ptr32_regptrTO_regFROM(g_out, 3, 4));
A64_CASE("subs_ptr32_regptrTO_regFROM",   emit_subs_ptr32_regptrTO_regFROM(g_out, 3, 4));
A64_CASE("sub_ptr32_regptrTO_immFROM",    emit_sub_ptr32_regptrTO_immFROM(g_out, 3, 0x1234));
A64_CASE("subs_ptr32_regptrTO_immFROM",   emit_subs_ptr32_regptrTO_immFROM(g_out, 3, 0x1234));
A64_CASE("neg_ptr32_regptr",              emit_neg_ptr32_regptr(g_out, 3));
A64_CASE("negs_ptr32_regptr",             emit_negs_ptr32_regptr(g_out, 3));
A64_CASE("ror_ptr32_regptr",              emit_ror_ptr32_regptr(g_out, 3, 7));
A64_CASE("rors_ptr32_regptr",             emit_rors_ptr32_regptr(g_out, 3, 7));
A64_CASE("rors_ptr32_regptr_reg",         emit_rors_ptr32_regptr_reg(g_out, 3, 4));
A64_CASE("ror_ptr32_regptr_reg",          emit_ror_ptr32_regptr_reg(g_out, 3, 4));
A64_CASE("and_ptr32_regptr",              emit_and_ptr32_regptr(g_out, 3, 0x1F));
A64_CASE("or_ptr32_regptr",               emit_or_ptr32_regptr(g_out, 3, 0x1F));
A64_CASE("or_ptr8_regptr_reg",            emit_or_ptr8_regptr_reg(g_out, 3, 4));
A64_CASE("or_ptr8_regptr_imm",            emit_or_ptr8_regptr_imm(g_out, 3, 0x1F));
A64_CASE("or_ptr32_regptr_reg",           emit_or_ptr32_regptr_reg(g_out, 3, 4));
A64_CASE("and_ptr32_regptr_reg",          emit_and_ptr32_regptr_reg(g_out, 3, 4));
A64_CASE("write_ptr32_reg_byte",          emit_write_ptr32_reg_byte(g_out, 0x12345678, 4));

// ------------------------------------------------------------------- setcc
A64_CASE("setc r2",  emit_setc(g_out, 2));
A64_CASE("sets r2",  emit_sets(g_out, 2));
A64_CASE("setz r2",  emit_setz(g_out, 2));
A64_CASE("setnz r2", emit_setnz(g_out, 2));
A64_CASE("setnc r2", emit_setnc(g_out, 2));
A64_CASE("seto r2",  emit_seto(g_out, 2));

// --------------------------------------------------------------------- lea
A64_CASE("lea_reg_reg 4",  emit_lea_reg_reg(g_out, 3, 4, 4, 2));
A64_CASE("lea_reg_reg 8",  emit_lea_reg_reg(g_out, 3, 4, 8, 2));
A64_CASE("lea_ptr_reg 4",  emit_lea_ptr_reg(g_out, 0x12345678, 4, 4, 2));
A64_CASE("lea_ptr_val 4",  emit_lea_ptr_val(g_out, 0x12345678, 5, 4, 2));

// ------------------------------------------------------------ carry flag bits
A64_CASE("clear_carry_flag", clear_carry_flag(g_out));
A64_CASE("set_carry_flag",   set_carry_flag(g_out));
A64_CASE("get_carry_flag",   get_carry_flag(g_out, 5));

// ------------------------------------------------------------- ptr helpers
A64_CASE("read_ptr32_reg",      emit_read_ptr32_reg(g_out, 0x12345678, 4));
A64_CASE("read_ptr64_reg",      emit_read_ptr64_reg(g_out, 0x12345678, 4));
A64_CASE("read_ptr32_reg_byte", emit_read_ptr32_reg_byte(g_out, 0x12345678, 4));
A64_CASE("var_copy32",          emit_var_copy32(g_out, 0x12345678, 0x1234567C));
A64_CASE("var_copy32_reg",      emit_var_copy32_reg(g_out, 3, 4));
A64_CASE("set_ptr_imm",         emit_set_ptr_imm(g_out, 0x12345678, 7));
A64_CASE("inc_ptr_imm +7",      emit_inc_ptr_imm(g_out, 0x12345678, 7));
A64_CASE("inc_ptr_imm -7",      emit_inc_ptr_imm(g_out, 0x12345678, -7));
A64_CASE("sub_ptr_imm 7",       emit_sub_ptr_imm(g_out, 0x12345678, 7));
A64_CASE("inc_ptr",             emit_inc_ptr(g_out, 0x12345678, 0x1234567C));
A64_CASE("inc_ptr_reg",         emit_inc_ptr_reg(g_out, 0x12345678, 0x1234567C, 5));
A64_CASE("inc_reg",             emit_inc_reg(g_out, 0x12345678, 5));

// ------------------------------------------------------ prolog, epilog, calls
A64_CASE("brk",            emit_brk(g_out));
A64_CASE("save",           save(g_out));
A64_CASE("restore",        restore(g_out));
A64_CASE("startfunc",      emit_startfunc(g_out));
A64_CASE("endfunc",        emit_endfunc(g_out));
A64_CASE("save_call",      save_call(g_out));
A64_CASE("restore_call",   restore_call(g_out));
A64_CASE("callR3",         callR3(g_out));
A64_CASE("callR9",         callR9(g_out));
A64_CASE("callR7",         callR7(g_out));
A64_CASE("callR6",         callR6(g_out));
A64_CASE("callR8",         callR8(g_out));

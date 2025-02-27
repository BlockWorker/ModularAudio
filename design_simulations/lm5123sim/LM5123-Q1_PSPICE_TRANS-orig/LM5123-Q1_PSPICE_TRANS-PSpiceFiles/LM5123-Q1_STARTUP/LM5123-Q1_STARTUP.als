.ALIASES
V_V1            V1(+=CSP -=0 ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS43119@SOURCE.VPWL.Normal(chips)
C_C22           C22(1=HB 2=SW ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS43017@ANALOG.C.Normal(chips)
R_R19           R19(1=0 2=TRK ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42921@ANALOG.R.Normal(chips)
R_RLoad          RLoad(1=0 2=VLOAD ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42823@ANALOG.R.Normal(chips)
L_L1            L1(1=CSN 2=SW ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42723@ANALOG.L.Normal(chips)
R_R7            R7(1=CSP 2=CSN ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS43039@ANALOG.R.Normal(chips)
R_R16           R16(1=UVLO 2=CSP ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS43141@ANALOG.R.Normal(chips)
C_C20           C20(1=0 2=SS ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42945@ANALOG.C.Normal(chips)
C_C16           C16(1=N36337 2=VLOAD ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42849@ANALOG.C.Normal(chips)
C_C19           C19(1=0 2=N37329 ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42751@ANALOG.C.Normal(chips)
X_S10    S10(1=LO 2=0 3=SW 4=0 ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42651@ANALOG.S.Normal(chips)
R_R8            R8(1=0 2=N36337 ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42967@ANALOG.R.Normal(chips)
C_C18           C18(1=0 2=COMP ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS43071@ANALOG.C.Normal(chips)
X_S11    S11(1=HO 2=SW 3=VLOAD 4=SW ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS43165@ANALOG.S.Normal(chips)
V_V2            V2(+=N41902 -=0 ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42871@SOURCE.VDC.Normal(chips)
D_D4            D4(A=0 C=SW ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42773@TPS40192APP.D_D1.Normal(chips)
D_D5            D5(A=SW C=VLOAD ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS43093@TPS40192APP.D_D1.Normal(chips)
R_R15           R15(1=N37329 2=COMP ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42993@ANALOG.R.Normal(chips)
R_R18           R18(1=TRK 2=VREF ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42895@ANALOG.R.Normal(chips)
R_R20           R20(1=N00155 2=N41902 ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42799@ANALOG.R.Normal(chips)
C_C21           C21(1=0 2=VCC ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS42701@ANALOG.C.Normal(chips)
R_R17           R17(1=0 2=UVLO ) CN @LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS43215@ANALOG.R.Normal(chips)
X_U1            U1(BIAS=CSP COMP=COMP CSN=CSN CSP=CSP HB=HB HO=HO LO=LO MODE=VCC PGOOD=N00155 SS=SS SW=SW TRK=TRK UVLO=UVLO
+VCC=VCC VH_CP=VCC VOUT=VLOAD VREF=VREF PGND=0 AGND=0 ) CN
+@LM5123-Q1_PSPICE_TRANS.LM5123-Q1_STARTUP(sch_1):INS16771427@LM5123-Q1_TRANS.LM5123-Q1_TRANS.Normal(chips)
_    _(COMP=COMP)
_    _(CSN=CSN)
_    _(CSP=CSP)
_    _(VIN=CSP)
_    _(HB=HB)
_    _(HO=HO)
_    _(LO=LO)
_    _(SS=SS)
_    _(SW=SW)
_    _(TRK=TRK)
_    _(UVLO=UVLO)
_    _(VCC=VCC)
_    _(VLOAD=VLOAD)
_    _(VREF=VREF)
.ENDALIASES

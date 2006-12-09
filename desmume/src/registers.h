#ifndef REGISTERS_H
#define REGISTERS_H

// Display Engine A
#define REG_DISPA_DISPCNT                       0x04000000
#define REG_DISPA_DISPSTAT                      0x04000004
#define REG_DISPA_VCOUNT                        0x04000006
#define REG_DISPA_BG0CNT                        0x04000008
#define REG_DISPA_BG1CNT                        0x0400000A
#define REG_DISPA_BG2CNT                        0x0400000C
#define REG_DISPA_BG3CNT                        0x0400000E
#define REG_DISPA_BG0HOFS                       0x04000010
#define REG_DISPA_BG0VOFS                       0x04000012
#define REG_DISPA_BG1HOFS                       0x04000014
#define REG_DISPA_BG1VOFS                       0x04000016
#define REG_DISPA_BG2HOFS                       0x04000018
#define REG_DISPA_BG2VOFS                       0x0400001A
#define REG_DISPA_BG3HOFS                       0x0400001C
#define REG_DISPA_BG3VOFS                       0x0400001E
#define REG_DISPA_BG2PA                         0x04000020
#define REG_DISPA_BG2PB                         0x04000022
#define REG_DISPA_BG2PC                         0x04000024
#define REG_DISPA_BG2PD                         0x04000026
#define REG_DISPA_BG2XL                         0x04000028
#define REG_DISPA_BG2XH                         0x0400002A
#define REG_DISPA_BG2YL                         0x0400002C
#define REG_DISPA_BG2YH                         0x0400002E
#define REG_DISPA_BG3PA                         0x04000030
#define REG_DISPA_BG3PB                         0x04000032
#define REG_DISPA_BG3PC                         0x04000034
#define REG_DISPA_BG3PD                         0x04000036
#define REG_DISPA_BG3XL                         0x04000038
#define REG_DISPA_BG3XH                         0x0400003A
#define REG_DISPA_BG3YL                         0x0400003C
#define REG_DISPA_BG3YH                         0x0400003E
#define REG_DISPA_WIN0H                         0x04000040
#define REG_DISPA_WIN1H                         0x04000042
#define REG_DISPA_WIN0V                         0x04000044
#define REG_DISPA_WIN1V                         0x04000046
#define REG_DISPA_WININ                         0x04000048
#define REG_DISPA_WINOUT                        0x0400004A
#define REG_DISPA_MOSAIC                        0x0400004C
#define REG_DISPA_BLDCNT                        0x04000050
#define REG_DISPA_BLDALPHA                      0x04000052
#define REG_DISPA_BLDY                          0x04000054
#define REG_DISPA_DISP3DCNT                     0x04000060
#define REG_DISPA_DISPCAPCNT                    0x04000064
#define REG_DISPA_DISPMMEMFIFO                  0x04000068
#define REG_DISPA_MASTERBRIGHT                  0x0400006C

// DMA
#define REG_DMA0SAD                             0x040000B0
#define REG_DMA0DAD                             0x040000B4
#define REG_DMA0CNTL                            0x040000B8
#define REG_DMA0CNTH                            0x040000BA
#define REG_DMA1SAD                             0x040000BC
#define REG_DMA1DAD                             0x040000C0
#define REG_DMA1CNTL                            0x040000C4
#define REG_DMA1CNTH                            0x040000C6
#define REG_DMA2SAD                             0x040000C8
#define REG_DMA2DAD                             0x040000CC
#define REG_DMA2CNTL                            0x040000D0
#define REG_DMA2CNTH                            0x040000D2
#define REG_DMA3SAD                             0x040000D4
#define REG_DMA3DAD                             0x040000D8
#define REG_DMA3CNTL                            0x040000DC
#define REG_DMA3CNTH                            0x040000DE
#define REG_DMA0FILL                            0x040000E0
#define REG_DMA1FILL                            0x040000E4
#define REG_DMA2FILL                            0x040000E8
#define REG_DMA3FILL                            0x040000EC

// Timers
#define REG_TM0CNTL                             0x04000100
#define REG_TM0CNTH                             0x04000102
#define REG_TM1CNTL                             0x04000104
#define REG_TM1CNTH                             0x04000106
#define REG_TM2CNTL                             0x04000108
#define REG_TM2CNTH                             0x0400010A
#define REG_TM3CNTL                             0x0400010C
#define REG_TM3CNTH                             0x0400010E

// SIO/Keypad Input/RTC
#define REG_SIODATA32                           0x04000120
#define REG_SIOCNT                              0x04000128
#define REG_KEYINPUT                            0x04000130
#define REG_KEYCNT                              0x04000132
#define REG_RCNT                                0x04000134
#define REG_EXTKEYIN                            0x04000136
#define REG_RTC                                 0x04000138

// IPC
#define REG_IPCSYNC                             0x04000180
#define REG_IPCFIFOCNT                          0x04000184
#define REG_IPCFIFOSEND                         0x04000188

// ROM
#define REG_AUXSPICNT                           0x040001A0
#define REG_AUXSPIDATA                          0x040001A2
#define REG_GCROMCTRL                           0x040001A4
#define REG_GCCMDOUT                            0x040001A8
#define REG_GCDATAIN                            0x04100010
#define REG_ENCSEED0L                           0x040001B0
#define REG_ENCSEED1L                           0x040001B4
#define REG_ENCSEED0H                           0x040001B8
#define REG_ENCSEED1H                           0x040001BC
#define REG_SPICNT                              0x040001C0
#define REG_SPIDATA                             0x040001C2

// Memory/IRQ
#define REG_EXMEMCNT                            0x04000204
#define REG_WIFIWAITCNT                         0x04000206
#define REG_IME                                 0x04000208
#define REG_IE                                  0x04000210
#define REG_IF                                  0x04000214
#define REG_VRAMCNTA                            0x04000240
#define REG_VRAMSTAT                            0x04000240
#define REG_VRAMCNTB                            0x04000241
#define REG_WRAMSTAT                            0x04000241
#define REG_VRAMCNTC                            0x04000242
#define REG_VRAMCNTD                            0x04000243
#define REG_VRAMCNTE                            0x04000244
#define REG_VRAMCNTF                            0x04000245
#define REG_VRAMCNTG                            0x04000246
#define REG_WRAMCNT                             0x04000247
#define REG_VRAMCNTH                            0x04000248
#define REG_VRAMCNTI                            0x04000249

// Math
#define REG_DIVCNT                              0x04000280
#define REG_DIVNUMER                            0x04000290
#define REG_DIVDENOM                            0x04000298
#define REG_DIVRESULT                           0x040002A0
#define REG_DIVREMRESULT                        0x040002A8
#define REG_SQRTCNT                             0x040002B0
#define REG_SQRTRESULT                          0x040002B4
#define REG_SQRTPARAM                           0x040002B8

// Other 
#define REG_POSTFLG                             0x04000300
#define REG_HALTCNT                             0x04000301
#define REG_POWCNT1                             0x04000304
#define REG_POWCNT2                             0x04000304
#define REG_BIOSPROT                            0x04000308

#define REG_DISPB_DISPCNT                       0x04001000
#define REG_DISPB_BG0CNT                        0x04001008
#define REG_DISPB_BG1CNT                        0x0400100A
#define REG_DISPB_BG2CNT                        0x0400100C
#define REG_DISPB_BG3CNT                        0x0400100E
#define REG_DISPB_BG0HOFS                       0x04001010
#define REG_DISPB_BG0VOFS                       0x04001012
#define REG_DISPB_BG1HOFS                       0x04001014
#define REG_DISPB_BG1VOFS                       0x04001016
#define REG_DISPB_BG2HOFS                       0x04001018
#define REG_DISPB_BG2VOFS                       0x0400101A
#define REG_DISPB_BG3HOFS                       0x0400101C
#define REG_DISPB_BG3VOFS                       0x0400101E
#define REG_DISPB_BG2PA                         0x04001020
#define REG_DISPB_BG2PB                         0x04001022
#define REG_DISPB_BG2PC                         0x04001024
#define REG_DISPB_BG2PD                         0x04001026
#define REG_DISPB_BG2XL                         0x04001028
#define REG_DISPB_BG2XH                         0x0400102A
#define REG_DISPB_BG2YL                         0x0400102C
#define REG_DISPB_BG2YH                         0x0400102E
#define REG_DISPB_BG3PA                         0x04001030
#define REG_DISPB_BG3PB                         0x04001032
#define REG_DISPB_BG3PC                         0x04001034
#define REG_DISPB_BG3PD                         0x04001036
#define REG_DISPB_BG3XL                         0x04001038
#define REG_DISPB_BG3XH                         0x0400103A
#define REG_DISPB_BG3YL                         0x0400103C
#define REG_DISPB_BG3YH                         0x0400103E
#define REG_DISPB_WIN0H                         0x04001040
#define REG_DISPB_WIN1H                         0x04001042
#define REG_DISPB_WIN0V                         0x04001044
#define REG_DISPB_WIN1V                         0x04001046
#define REG_DISPB_WININ                         0x04001048
#define REG_DISPB_WINOUT                        0x0400104A
#define REG_DISPB_MOSAIC                        0x0400104C
#define REG_DISPB_BLDCNT                        0x04001050
#define REG_DISPB_BLDALPHA                      0x04001052
#define REG_DISPB_BLDY                          0x04001054
#define REG_DISPB_MASTERBRIGHT                  0x0400106C

#endif

copy /y winamp2\plugins\vio2sf.bin ~dist
copy /y winamp2\plugins\in_vio2sf.dll ~dist
copy /y winamp5\plugins\in_vio2sfu.dll ~dist
copy /y fb8\foo_8_vio2sf.dll ~dist
copy /y fb9\components\foo_input_vio2sf.dll ~dist
copy /y kbmed\plugins\vio2sf.kpi ~dist
upx -9 ~dist/*
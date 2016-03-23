# LABEL 0ESPOL 0COMPILE00186059? EXECUTE ESPOL/DISK; FILE DISK=CHARMOD/TEST SERIAL ESPOL /DISK
# 
# 
# 
# 
# 
# BURROUGHS B-5700 ESPOL COMPILER MARK XIII.0 FRIDAY, 02/28/86, 9:20 AM.
# 
# 
# 
# 
# 
# %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% 00000200 0000:0
# %% %% 00000300 0000:0
# %% RETRO-B5500 EMULATOR CHARACTER MODE TESTS %% 00000400 0000:0
# %% %% 00000500 0000:0
# %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% 00000600 0000:0
# % 2013-01-26 P.KIMPEL 00000700 0000:0
# % ORIGINAL VERSION 00000800 0000:0
# %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% 00099900 0000:0
# 00100000 0000:0
# BEGIN 00100100 0000:0
# INTEGER I; 00100200 0000:0

#PRT(201) = *SEGMENT DESCRIPTOR*
	.ORG	0201
	.WORD	0

#PRT(202) = I
	.ORG	0202
	.WORD	0
# REAL R; 00100300 0000:0

#PRT(203) = R
	.ORG	0203
	.WORD	0
# ARRAY S[16]:= 00100400 0000:0

#PRT(204) = S
	.ORG	0204
	.WORD	05000200000001000	# array S at @1000 for 16
# START OF SAVE SEGMENT; BASE ADDRESS = 00000
# "01234567","89ABCDEF","GHIJKLMN","OPQRSTUV","WXYZ +-/", 00100500 0000:0
# "NOW IS T","HE TIME ","FOR ALL ","GOOD MEN"," TO COME", 00100600 0000:0
# " TO THE ","AID OF T","HEIR PAR","TY.     ","1234567Q","12345678"; 00100700 0000:0
# SIZE= 0016 WORDS
	.ORG	01020
	.WORD	"01234567"
	.WORD	"89ABCDEF"
	.WORD	"GHIJKLMN"
	.WORD	"OPQRSTUV"
	.WORD	"WXYZ +-/"
	.WORD	"NOW IS T"
	.WORD	"HE TIME "
	.WORD	"FOR ALL "
	.WORD	"GOOD MEN"
	.WORD	" TO COME"
	.WORD	" TO THE "
	.WORD	"AID OF T"
	.WORD	"HEIR PAR"
	.WORD	"TY.     "
	.WORD	"1234567Q"
	.WORD	"12345678"
# ARRAY D[16]; 00100800 0000:0

#PRT(205) = D
	.ORG	0205
	.WORD	05000200000001020	# array D at @1020 for 16
# START OF SAVE SEGMENT; BASE ADDRESS = 00016
# SIZE= 0016 WORDS
	.ORG	01020
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
	.WORD	0
# LABEL ENTRY, START; 00100900 0000:0
# 00400000 0000:0

# ENTRY:@20: GO TO START; 00400100 0000:0
	.ORG	020
# 0100 LITC 0000 0000
	LITC	0
# 0101 OPDC 0211 1046
	OPDC	0211
# 00500000 0016:2

# START:*: 00500100 0016:2
	.ORG	020
# 00500200 0134:0

# D[0]:= 76543210; 00500300 0134:0
# 0100 LITC 0166 0730
	LITC	0166
# 0101 LFU 6231
	LFU

	.ORG	0206
# 1030 LITC 0000 0000
	LITC	0	# index 0
# 1031 DESC 0205 1027
	DESC	0205	# addr of array D
# 1032 DESC 1777 7777 PATCHED LATER BY
# 1032 OPDC 0265 1326
	OPDC	0265	# addr of constant 76543210
# 1033 NOP 0055
	NOP
# 1034 XCH 1025
	XCH
# 1035 STD 0421
	STD

# 00520000 0135:2
# STREAM(R:=-D[0] : COUNT:=0, S:=S, D:=D); 00520100 0135:2
# 1036 NOP 0055
	NOP
# 1037 LITC 0000 0000
	LITC	0	# index 0
# 1040 OPDC 0205 1026
	OPDC	0205	# D
# 1041 CHS 1031
	CHS		# -
# 1042 MKS 0441
	MKS
# 1043 LITC 0000 0000
	LITC	0
# 1044 LITC 0204 1020
	LITC	0204	# S
# 1045 LOD 2021
	LOD
# 1046 LITC 0205 1024
	LITC	0205	# D
# 1047 LOD 2021
	LOD
# 1050 4441
	CMN
# BEGIN 00520200 0138:1
# SI:= S; 00520300 0138:1
# 1051 RSA 0002 0253
	RSA	2
# SI:= SI+51; 00520400 0138:2
# 1052 SFS 0063 6331
	SFS	51
# DI:= DI+6; 00520500 0138:3
# 1053 SFD 0006 0616
	SFD	6
# DS:= 5 CHR; 00520600 0139:0
# 1054 TRS 0005 0577
	TRS	5
# 00520700 0139:1
# SI:= LOC R; 00520800 0139:1
# 1055 SES 0005 0522
	SES	5
# DI:= D; 00520900 0139:2
# 1056 RDA 0001 0104
	RDA	1
# DI:= DI+17; 00521000 0139:3
# 1057 SFD 0021 2116
	SFD	17
# DS:= 8 DEC; 00521100 0140:0
# 1060 OCV 0010 1066
	OCV	8
# 00521200 0140:1
# SI:= D; 00521300 0140:1
# 1061 RSA 0001 0153
	RSA	1
# 2(SI:= SI+7); 00521400 0140:2
# 1062 BNS 0002 0252
	BNS	2
# 1063 SFS 0007 0731
	SFS	7
# 1064 ENS 0000 0051
	ENS	0
# SI:= SI+3; 00521500 0141:1
# 1065 SFS 0003 0331
	SFS	3
# DI:= LOC R; 00521600 0141:2
# 1066 SED 0005 0506
	SED	5
# DS:= 8 OCT; 00521700 0141:3
# 1067 ICV 0010 1067
	ICV	8
# 00521800 0142:0
# DI:= D; 00521900 0142:0
# 1070 RDA 0001 0104
	RDA	1
# DI:= DI+9; 00522000 0142:1
# 1071 SFD 0011 1116
	SFD	9
# COUNT:= DI; 00522100 0142:2
# 1072 SDA 0003 0314
	SDA	3
# DS:= 9 LIT "ADGJMPSV7"; 00522200 0142:3
# 1073 TRP 0011 1174
	TRP	9
# 1074 0 0000 0021
# 1075 TGR 0024 2427
	TGR	024
# 1076 JNC 0041 4144
	JNC	041
# 1077 CEG 0047 4762
	CEG	047
# 1100 TDA 0065 6507
	TDA	065
# 00522300 0144:1
# SI:= COUNT; 00522400 0144:1
# 1101 RSA 0003 0353
	RSA	3
# DS:= 9 ZON; 00522500 0144:2
# 1102 TRZ 0011 1176
	TRZ	9
# 00522600 0144:3
# SI:= COUNT; 00522700 0144:3
# 1103 RSA 0003 0353
	RSA	3
# DS:= 9 NUM; 00522800 0145:0
# 1104 TRN 0011 1175
	TRN	9
# 00522900 0145:1
# SI:= COUNT; 00523000 0145:1
# 1105 RSA 0003 0353
	RSA	3
# DI:= COUNT; 00523100 0145:2
# 1106 RDA 0003 0304
	RDA	3
# DI:= DI+11; 00523200 0145:3
# 1107 SFD 0013 1316
	SFD	11
# TALLY:= 3; 00523300 0146:0
# 1110 SEC 0003 0342
	SEC	3
# COUNT:= TALLY; 00523400 0146:1
# 1111 STC 0003 0341
	STC	3
# DS:= COUNT WDS; 00523500 0146:2
# 1112 CRF 0003 0343
	CRF	3
# 1113 TRW 0000 0005
	TRW	0
# 00523600 0147:0
# DI:= D; 00523700 0147:0
# 1114 RDA 0001 0104
	RDA	1
# DI:= DI+9; 00523800 0147:1
# 1115 SFD 0011 1116
	SFD	9
# DS:= 9 FILL; 00523900 0147:2
# 1116 0 0011 1112
# 00524000 0147:3
# SI:= S; 00524100 0147:3
# 1117 RSA 0002 0253
	RSA	2
# IF SC > "A" THEN 00524200 0148:0
# 1120 TGR 0021 2127
	TGR	021
# 1121 JFC 0000 0045
	JFC	0
# DS:= SET; 00524300 0148:2
# 1122 BIT 0001 0164
	BIT	1
# 1121 JFC 0001 0145
	JFC	1
# IF SC = "A" THEN 00524400 0148:3
# 1123 TEG 0021 2126
	TEG	021
# 1124 JFC 0000 0045
	JFC	0
# DS:= SET; 00524500 0149:1
# 1125 BIT 0001 0164
	BIT	1
# 1124 JFC 0001 0145
# IF SC = "A" THEN 00524600 0149:2
# 1126 TEQ 0021 2124
	TEQ	021
# 1127 JFC 0000 0045
	JFC	0
# DS:= SET; 00524700 0150:0
# 1130 BIT 0001 0164
	BIT	1
# 1127 JFC 0001 0145
	JFC	1
# IF SC = "A" THEN 00524800 0150:1
# 1131 TEL 0021 2134
	TEL	021
# 1132 JFC 0000 0045
	JFC	0
# DS:= SET; 00524900 0150:3
# 1133 BIT 0001 0164
	BIT	1
# 1132 JFC 0001 0145
# IF SC < "A" THEN 00525000 0151:0
# 1134 TLS 0021 2135
# 1135 JFC 0000 0045
# DS:= SET; 00525100 0151:2
# 1136 BIT 0001 0164
# 1135 JFC 0001 0145
# IF SC ? "A" THEN 00525200 0151:3
# 1137 TNE 0021 2125
# 1140 JFC 0000 0045
# DS:= SET; 00525300 0152:1
# 1141 BIT 0001 0164
# 1140 JFC 0001 0145
# IF SC = ALPHA THEN 00525400 0152:2
# 1142 TAN 0021 2136
# 1143 JFC 0000 0045
# DS:= SET; 00525500 0153:0
# 1144 BIT 0001 0164
# 1143 JFC 0001 0145
# 00525600 0153:1
# SI:= S; 00525700 0153:1
# 1145 RSA 0002 0253
# DI:= D; 00525800 0153:2
# 1146 RDA 0001 0104
# IF 9 SC > DC THEN 00525900 0153:3
# 1147 CGR 0011 1163
# 1150 JFC 0000 0045
# TALLY:= TALLY+1; 00526000 0154:1
# 1151 INC 0001 0140
# 1150 JFC 0001 0145
# SI:= S; 00526100 0154:2
# 1152 RSA 0002 0253
# DI:= D; 00526200 0154:3
# 1153 RDA 0001 0104
# IF 9 SC = DC THEN 00526300 0155:0
# 1154 CEG 0011 1162
# 1155 JFC 0000 0045
# TALLY:= TALLY+1; 00526400 0155:2
# 1156 INC 0001 0140
# 1155 JFC 0001 0145
# SI:= S; 00526500 0155:3
# 1157 RSA 0002 0253
# DI:= D; 00526600 0156:0
# 1160 RDA 0001 0104
# IF 9 SC = DC THEN 00526700 0156:1
# 1161 CEQ 0011 1160
# 1162 JFC 0000 0045
# TALLY:= TALLY+1; 00526800 0156:3
# 1163 INC 0001 0140
# 1162 JFC 0001 0145
# SI:= S; 00526900 0157:0
# 1164 RSA 0002 0253
# DI:= D; 00527000 0157:1
# 1165 RDA 0001 0104
# IF 9 SC = DC THEN 00527100 0157:2
# 1166 CEL 0011 1170
# 1167 JFC 0000 0045
# TALLY:= TALLY+1; 00527200 0158:0
# 1170 INC 0001 0140
# 1167 JFC 0001 0145
# SI:= S; 00527300 0158:1
# 1171 RSA 0002 0253
# DI:= D; 00527400 0158:2
# 1172 RDA 0001 0104
# IF 9 SC < DC THEN 00527500 0158:3
# 1173 CLS 0011 1171
# 1174 JFC 0000 0045
# TALLY:= TALLY+1; 00527600 0159:1
# 1175 INC 0001 0140
# 1174 JFC 0001 0145
# SI:= S; 00527700 0159:2
# 1176 RSA 0002 0253
# DI:= D; 00527800 0159:3
# 1177 RDA 0001 0104
# IF 9 SC ? DC THEN 00527900 0160:0
# 1200 CNE 0011 1161
# 1201 JFC 0000 0045
# TALLY:= TALLY+1; 00528000 0160:2
# 1202 INC 0001 0140
# 1201 JFC 0001 0145
# 00528100 0160:3
# 3(IF SB THEN JUMP OUT ELSE SKIP SB); 00528200 0160:3
# 1203 BNS 0003 0352
# 1204 BIT 0001 0137
# 1205 JFC 0000 0045
# 1206 JNS 0000 0046
# 1207 TRS 0077 7777
# 1210 JFW 0000 0047
# 1205 JFC 0003 0345
# 1211 BSS 0001 0103
# 1210 JFW 0001 0147
# 1212 ENS 0000 0051
# 1207 JFW 0003 0347
# 2(IF SB THEN 00528300 0162:3
# 1213 BNS 0002 0252
# 1214 BIT 0001 0137
# 1215 JFC 0000 0045
# SKIP SB 00528400 0163:2
# 1216 BSS 0001 0103
# ELSE 00528500 0163:3
# 1217 JFW 0000 0047
# 1215 JFC 0002 0245
# BEGIN 00528600 0164:0
# SKIP 2 SB; 00528700 0164:0
# 1220 BSS 0002 0203
# JUMP OUT; 00528800 0164:1
# 1221 JNS 0000 0046
# 1222 TRS 0077 7777
# END; 00528900 0164:3
# 1217 JFW 0003 0347
# ); 00529000 0164:3
# 1223 ENS 0000 0051
# 1222 JFW 0001 0147
# 00529100 0165:0
# SI:= SC; 00529200 0165:0
# 1224 TSA 0000 0056
# DI:= DC; 00529300 0165:1
# 1225 TDA 0000 0007
# 00529400 0165:2
# SI:= S; 00529500 0165:2
# 1226 RSA 0002 0253
# DI:= D; 00529600 0165:3
# 1227 RDA 0001 0104
# DI:= DI+8; 00529700 0166:0
# 1230 SFD 0010 1016
# SKIP 40 SB; 00529800 0166:1
# 1231 BSS 0050 5003
# SKIP 40 DB; 00529900 0166:2
# 1232 BSD 0050 5002
# TALLY:= 10; 00530000 0166:3
# 1233 SEC 0012 1242
# COUNT:= TALLY; 00530100 0167:0
# 1234 STC 0003 0341
# COUNT( 00530200 0167:1
# 1235 CRF 0003 0343
# 1236 BNS 0000 0052
# IF SB THEN DS:= SET ELSE DS:= RESET; 00530300 0167:3
# 1237 BIT 0001 0137
# 1240 JFC 0000 0045
# 1241 BIT 0001 0164
# 1242 JFW 0000 0047
# 1240 JFC 0002 0245
# 1243 BIR 0001 0165
# 1242 JFW 0001 0147
# SKIP SB; 00530400 0169:0
# 1244 BSS 0001 0103
# ); 00530500 0169:1
# 1245 ENS 0000 0051
# 1236 BNS 0007 0752
# 00530600 0169:2
# DI:= D; 00530700 0169:2
# 1246 RDA 0001 0104
# DS:= 9 LIT "00000123M"; 00530800 0169:3
# 1247 TRP 0011 1174
# 1250 EXC 0000 0000
# 1251 EXC 0000 0000
# 1252 EXC 0000 0000
# 1253 BSD 0001 0102
# 1254 JNC 0003 0344
# DS:= 9 LIT "765432100"; 00530900 0171:1
# 1255 TRP 0011 1174
# 1256 TDA 0000 0007
# 1257 TRW 0006 0605
# 1260 BSS 0004 0403
# 1261 0 0002 0201
# 1262 EXC 0000 0000
# SI:= D; 00531000 0172:3
# 1263 RSA 0001 0153
# DI:= D; 00531100 0173:0
# 1264 RDA 0001 0104
# DI:= DI+9; 00531200 0173:1
# 1265 SFD 0011 1116
# DS:= 9 ADD; 00531300 0173:2
# 1266 FAD 0011 1173
# 00531400 0173:3
# DI:= D; 00531500 0173:3
# 1267 RDA 0001 0104
# DS:= 9 LIT "00000123M"; 00531600 0174:0
# 1270 TRP 0011 1174
# 1271 EXC 0000 0000
# 1272 EXC 0000 0000
# 1273 EXC 0000 0000
# 1274 BSD 0001 0102
# 1275 JNC 0003 0344
# DS:= 9 LIT "765432100"; 00531700 0175:2
# 1276 TRP 0011 1174
# 1277 TDA 0000 0007
# 1300 TRW 0006 0605
# 1301 BSS 0004 0403
# 1302 0 0002 0201
# 1303 EXC 0000 0000
# SI:= D; 00531800 0177:0
# 1304 RSA 0001 0153
# DI:= D; 00531900 0177:1
# 1305 RDA 0001 0104
# DI:= DI+9; 00532000 0177:2
# 1306 SFD 0011 1116
# DS:= 9 SUB; 00532100 0177:3
# 1307 FSU 0011 1172
# 00532200 0178:0
# COUNT:= CI; 00532300 0178:0
# 1310 SCA 0003 0354
# CI:= COUNT; % SINCE RCA INCREMENTS L, THIS SHOULD BE OK 00532400 0178:1
# 1311 RCA 0003 0350
# TALLY:= COUNT; 00532500 0178:2
# 1312 SEC 0000 0042
# 1313 CRF 0003 0343
# 1314 SEC 0000 0042
# 00532600 0179:1
# END STREAM; 00549900 0179:1
# 1315 LITC 0020 0100
# 00550000 0179:2
# P(DEL); % EAT THE WORD LEFT BELOW MSCW BY THE STREAM 00550100 0179:2
# 1316 0051
# 00999700 0179:3
# GO TO START; 00999800 0179:3
# 1317 LITC 0056 0270
# 1320 LBU 6131
# 1321 NOP 0055
# 1322 NOP 0055
# 1323 NOP 0055
	.ORG	0265	# constant 76543210
	.WORD	76543210
# 1032 OPDC 0265 1326
#1324 0000000443772352 00999800 0179:3
# END. 00999900 0182:0
# SIZE= 0182 WORDS
#NUMBER OF ERRORS DETECTED = 000. COMPILATION TIME = 0012 SECONDS.
#PRT SIZE=134 BASE ADDRESS=0182 CORE REQ=0214 DISK REQ=00300
# 
# 
# 
# 
# 
# LABEL 0ESPOL 0COMPILE00186059? EXECUTE ESPOL/DISK; FILE DISK=CHARMOD/TEST SERIAL ESPOL /DISK
	OPDC	01020
	ZPI
	.ORG	020
	.RUN
	.END

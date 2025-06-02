; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str3 = internal constant [19 x i8] c"Result: INCORRECT\0A\00"
@str2 = internal constant [17 x i8] c"Result: CORRECT\0A\00"
@str1 = internal constant [51 x i8] c"Mismatch at (%d,%d): Parallel=%.2f vs Serial=%.2f\0A\00"
@str0 = internal constant [24 x i8] c"Finished in %f seconds\0A\00"

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

declare void @artsShutdown()

declare i64 @artsGetCurrentEpochGuid()

declare void @artsDbIncrementLatch(i64)

declare void @artsDbAddDependence(i64, i64, i32)

declare i64 @artsGetCurrentGuid()

declare i64 @artsEdtCreateWithEpoch(ptr, i32, i32, ptr, i32, i64)

declare i1 @artsWaitOnHandle(i64)

declare i64 @artsInitializeAndStartEpoch(i64, i32)

declare i64 @artsEdtCreate(ptr, i32, i32, ptr, i32)

declare ptr @artsDbCreateWithGuid(i64, i64)

declare i64 @artsReserveGuidRoute(i32, i32)

declare i32 @artsGetCurrentNode()

declare i32 @printf(ptr, ...)

define i32 @mainBody(i32 %0, ptr %1) {
  %3 = alloca [196 x double], i64 196, align 8
  %4 = call i32 @artsGetCurrentNode()
  %5 = alloca [196 x i64], i64 196, align 8
  %6 = alloca [196 x ptr], i64 196, align 8
  br label %7

7:                                                ; preds = %20, %2
  %8 = phi i64 [ 0, %2 ], [ %21, %20 ]
  %9 = icmp slt i64 %8, 196
  br i1 %9, label %10, label %22

10:                                               ; preds = %7
  br label %11

11:                                               ; preds = %14, %10
  %12 = phi i64 [ 0, %10 ], [ %19, %14 ]
  %13 = icmp slt i64 %12, 196
  br i1 %13, label %14, label %20

14:                                               ; preds = %11
  %15 = call i64 @artsReserveGuidRoute(i32 10, i32 %4)
  %16 = call ptr @artsDbCreateWithGuid(i64 %15, i64 8)
  %17 = getelementptr [196 x i64], ptr %5, i64 %8, i64 %12
  store i64 %15, ptr %17, align 8
  %18 = getelementptr [196 x ptr], ptr %6, i64 %8, i64 %12
  store ptr %16, ptr %18, align 8
  %19 = add i64 %12, 1
  br label %11

20:                                               ; preds = %11
  %21 = add i64 %8, 1
  br label %7

22:                                               ; preds = %7
  %23 = call i32 @artsGetCurrentNode()
  %24 = alloca [5 x i64], i64 5, align 8
  %25 = alloca [5 x ptr], i64 5, align 8
  br label %26

26:                                               ; preds = %39, %22
  %27 = phi i64 [ 0, %22 ], [ %40, %39 ]
  %28 = icmp slt i64 %27, 5
  br i1 %28, label %29, label %41

29:                                               ; preds = %26
  br label %30

30:                                               ; preds = %33, %29
  %31 = phi i64 [ 0, %29 ], [ %38, %33 ]
  %32 = icmp slt i64 %31, 5
  br i1 %32, label %33, label %39

33:                                               ; preds = %30
  %34 = call i64 @artsReserveGuidRoute(i32 8, i32 %23)
  %35 = call ptr @artsDbCreateWithGuid(i64 %34, i64 8)
  %36 = getelementptr [5 x i64], ptr %24, i64 %27, i64 %31
  store i64 %34, ptr %36, align 8
  %37 = getelementptr [5 x ptr], ptr %25, i64 %27, i64 %31
  store ptr %35, ptr %37, align 8
  %38 = add i64 %31, 1
  br label %30

39:                                               ; preds = %30
  %40 = add i64 %27, 1
  br label %26

41:                                               ; preds = %26
  %42 = call i32 @artsGetCurrentNode()
  %43 = alloca [200 x i64], i64 200, align 8
  %44 = alloca [200 x ptr], i64 200, align 8
  br label %45

45:                                               ; preds = %58, %41
  %46 = phi i64 [ 0, %41 ], [ %59, %58 ]
  %47 = icmp slt i64 %46, 200
  br i1 %47, label %48, label %60

48:                                               ; preds = %45
  br label %49

49:                                               ; preds = %52, %48
  %50 = phi i64 [ 0, %48 ], [ %57, %52 ]
  %51 = icmp slt i64 %50, 200
  br i1 %51, label %52, label %58

52:                                               ; preds = %49
  %53 = call i64 @artsReserveGuidRoute(i32 8, i32 %42)
  %54 = call ptr @artsDbCreateWithGuid(i64 %53, i64 8)
  %55 = getelementptr [200 x i64], ptr %43, i64 %46, i64 %50
  store i64 %53, ptr %55, align 8
  %56 = getelementptr [200 x ptr], ptr %44, i64 %46, i64 %50
  store ptr %54, ptr %56, align 8
  %57 = add i64 %50, 1
  br label %49

58:                                               ; preds = %49
  %59 = add i64 %46, 1
  br label %45

60:                                               ; preds = %45
  br label %61

61:                                               ; preds = %74, %60
  %62 = phi i64 [ 0, %60 ], [ %75, %74 ]
  %63 = icmp slt i64 %62, 200
  br i1 %63, label %64, label %76

64:                                               ; preds = %61
  %65 = getelementptr [200 x ptr], ptr %44, i64 %62, i32 0
  br label %66

66:                                               ; preds = %69, %64
  %67 = phi i64 [ 0, %64 ], [ %73, %69 ]
  %68 = icmp slt i64 %67, 200
  br i1 %68, label %69, label %74

69:                                               ; preds = %66
  %70 = mul i64 %67, 8
  %71 = getelementptr i8, ptr %65, i64 %70
  %72 = load ptr, ptr %71, align 8
  store double 1.000000e+00, ptr %72, align 8
  %73 = add i64 %67, 1
  br label %66

74:                                               ; preds = %66
  %75 = add i64 %62, 1
  br label %61

76:                                               ; preds = %61
  br label %77

77:                                               ; preds = %90, %76
  %78 = phi i64 [ 0, %76 ], [ %91, %90 ]
  %79 = icmp slt i64 %78, 5
  br i1 %79, label %80, label %92

80:                                               ; preds = %77
  %81 = getelementptr [5 x ptr], ptr %25, i64 %78, i32 0
  br label %82

82:                                               ; preds = %85, %80
  %83 = phi i64 [ 0, %80 ], [ %89, %85 ]
  %84 = icmp slt i64 %83, 5
  br i1 %84, label %85, label %90

85:                                               ; preds = %82
  %86 = mul i64 %83, 8
  %87 = getelementptr i8, ptr %81, i64 %86
  %88 = load ptr, ptr %87, align 8
  store double 1.000000e+00, ptr %88, align 8
  %89 = add i64 %83, 1
  br label %82

90:                                               ; preds = %82
  %91 = add i64 %78, 1
  br label %77

92:                                               ; preds = %77
  %93 = call double @omp_get_wtime()
  %94 = call i32 @artsGetCurrentNode()
  %95 = alloca i64, i64 0, align 8
  %96 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %94, i32 0, ptr %95, i32 1)
  %97 = call i64 @artsInitializeAndStartEpoch(i64 %96, i32 0)
  %98 = call i32 @artsGetCurrentNode()
  %99 = alloca i64, i64 0, align 8
  %100 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %98, i32 0, ptr %99, i32 78441, i64 %97)
  %101 = call ptr @malloc(i64 8)
  store i64 0, ptr %101, align 8
  br label %102

102:                                              ; preds = %116, %92
  %103 = phi i64 [ 0, %92 ], [ %117, %116 ]
  %104 = icmp slt i64 %103, 196
  br i1 %104, label %105, label %118

105:                                              ; preds = %102
  br label %106

106:                                              ; preds = %109, %105
  %107 = phi i64 [ 0, %105 ], [ %115, %109 ]
  %108 = icmp slt i64 %107, 196
  br i1 %108, label %109, label %116

109:                                              ; preds = %106
  %110 = load i64, ptr %101, align 8
  %111 = getelementptr [196 x i64], ptr %5, i64 %103, i64 %107
  %112 = load i64, ptr %111, align 8
  %113 = trunc i64 %110 to i32
  call void @artsDbAddDependence(i64 %112, i64 %100, i32 %113)
  %114 = add i64 %110, 1
  store i64 %114, ptr %101, align 8
  %115 = add i64 %107, 1
  br label %106

116:                                              ; preds = %106
  %117 = add i64 %103, 1
  br label %102

118:                                              ; preds = %102
  %119 = call ptr @malloc(i64 8)
  store i64 38416, ptr %119, align 8
  br label %120

120:                                              ; preds = %134, %118
  %121 = phi i64 [ 0, %118 ], [ %135, %134 ]
  %122 = icmp slt i64 %121, 5
  br i1 %122, label %123, label %136

123:                                              ; preds = %120
  br label %124

124:                                              ; preds = %127, %123
  %125 = phi i64 [ 0, %123 ], [ %133, %127 ]
  %126 = icmp slt i64 %125, 5
  br i1 %126, label %127, label %134

127:                                              ; preds = %124
  %128 = load i64, ptr %119, align 8
  %129 = getelementptr [5 x i64], ptr %24, i64 %121, i64 %125
  %130 = load i64, ptr %129, align 8
  %131 = trunc i64 %128 to i32
  call void @artsDbAddDependence(i64 %130, i64 %100, i32 %131)
  %132 = add i64 %128, 1
  store i64 %132, ptr %119, align 8
  %133 = add i64 %125, 1
  br label %124

134:                                              ; preds = %124
  %135 = add i64 %121, 1
  br label %120

136:                                              ; preds = %120
  %137 = call ptr @malloc(i64 8)
  store i64 38441, ptr %137, align 8
  br label %138

138:                                              ; preds = %152, %136
  %139 = phi i64 [ 0, %136 ], [ %153, %152 ]
  %140 = icmp slt i64 %139, 200
  br i1 %140, label %141, label %154

141:                                              ; preds = %138
  br label %142

142:                                              ; preds = %145, %141
  %143 = phi i64 [ 0, %141 ], [ %151, %145 ]
  %144 = icmp slt i64 %143, 200
  br i1 %144, label %145, label %152

145:                                              ; preds = %142
  %146 = load i64, ptr %137, align 8
  %147 = getelementptr [200 x i64], ptr %43, i64 %139, i64 %143
  %148 = load i64, ptr %147, align 8
  %149 = trunc i64 %146 to i32
  call void @artsDbAddDependence(i64 %148, i64 %100, i32 %149)
  %150 = add i64 %146, 1
  store i64 %150, ptr %137, align 8
  %151 = add i64 %143, 1
  br label %142

152:                                              ; preds = %142
  %153 = add i64 %139, 1
  br label %138

154:                                              ; preds = %138
  %155 = call ptr @malloc(i64 8)
  store i64 0, ptr %155, align 8
  br label %156

156:                                              ; preds = %169, %154
  %157 = phi i64 [ 0, %154 ], [ %170, %169 ]
  %158 = icmp slt i64 %157, 196
  br i1 %158, label %159, label %171

159:                                              ; preds = %156
  br label %160

160:                                              ; preds = %163, %159
  %161 = phi i64 [ 0, %159 ], [ %168, %163 ]
  %162 = icmp slt i64 %161, 196
  br i1 %162, label %163, label %169

163:                                              ; preds = %160
  %164 = getelementptr [196 x i64], ptr %5, i64 %157, i64 %161
  %165 = load i64, ptr %164, align 8
  call void @artsDbIncrementLatch(i64 %165)
  %166 = load i64, ptr %155, align 8
  %167 = add i64 %166, 1
  store i64 %167, ptr %155, align 8
  %168 = add i64 %161, 1
  br label %160

169:                                              ; preds = %160
  %170 = add i64 %157, 1
  br label %156

171:                                              ; preds = %156
  %172 = call i1 @artsWaitOnHandle(i64 %97)
  %173 = call double @omp_get_wtime()
  %174 = fsub double %173, %93
  %175 = call i32 (ptr, ...) @printf(ptr @str0, double %174)
  br label %176

176:                                              ; preds = %228, %171
  %177 = phi i64 [ 0, %171 ], [ %229, %228 ]
  %178 = icmp slt i64 %177, 196
  br i1 %178, label %179, label %230

179:                                              ; preds = %176
  %180 = trunc i64 %177 to i32
  br label %181

181:                                              ; preds = %225, %179
  %182 = phi i64 [ 0, %179 ], [ %227, %225 ]
  %183 = icmp slt i64 %182, 196
  br i1 %183, label %184, label %228

184:                                              ; preds = %181
  %185 = trunc i64 %182 to i32
  br label %186

186:                                              ; preds = %223, %184
  %187 = phi i64 [ 0, %184 ], [ %224, %223 ]
  %188 = phi double [ 0.000000e+00, %184 ], [ %196, %223 ]
  %189 = icmp slt i64 %187, 5
  br i1 %189, label %190, label %225

190:                                              ; preds = %186
  %191 = trunc i64 %187 to i32
  %192 = add i32 %180, %191
  %193 = icmp slt i32 %192, 200
  br label %194

194:                                              ; preds = %221, %190
  %195 = phi i64 [ 0, %190 ], [ %222, %221 ]
  %196 = phi double [ %188, %190 ], [ %220, %221 ]
  %197 = icmp slt i64 %195, 5
  br i1 %197, label %198, label %223

198:                                              ; preds = %194
  %199 = trunc i64 %195 to i32
  %200 = add i32 %185, %199
  %201 = icmp slt i32 %200, 200
  %202 = and i1 %193, %201
  br i1 %202, label %203, label %218

203:                                              ; preds = %198
  %204 = sext i32 %192 to i64
  %205 = sext i32 %200 to i64
  %206 = getelementptr [200 x ptr], ptr %44, i64 %204, i32 0
  %207 = mul i64 %205, 8
  %208 = getelementptr i8, ptr %206, i64 %207
  %209 = load ptr, ptr %208, align 8
  %210 = load double, ptr %209, align 8
  %211 = getelementptr [5 x ptr], ptr %25, i64 %187, i32 0
  %212 = mul i64 %195, 8
  %213 = getelementptr i8, ptr %211, i64 %212
  %214 = load ptr, ptr %213, align 8
  %215 = load double, ptr %214, align 8
  %216 = fmul double %210, %215
  %217 = fadd double %196, %216
  br label %219

218:                                              ; preds = %198
  br label %219

219:                                              ; preds = %203, %218
  %220 = phi double [ %196, %218 ], [ %217, %203 ]
  br label %221

221:                                              ; preds = %219
  %222 = add i64 %195, 1
  br label %194

223:                                              ; preds = %194
  %224 = add i64 %187, 1
  br label %186

225:                                              ; preds = %186
  %226 = getelementptr [196 x double], ptr %3, i64 %177, i64 %182
  store double %188, ptr %226, align 8
  %227 = add i64 %182, 1
  br label %181

228:                                              ; preds = %181
  %229 = add i64 %177, 1
  br label %176

230:                                              ; preds = %176
  br label %231

231:                                              ; preds = %261, %230
  %232 = phi i64 [ 0, %230 ], [ %262, %261 ]
  %233 = phi i32 [ 0, %230 ], [ %239, %261 ]
  %234 = icmp slt i64 %232, 196
  br i1 %234, label %235, label %263

235:                                              ; preds = %231
  %236 = getelementptr [196 x ptr], ptr %6, i64 %232, i32 0
  br label %237

237:                                              ; preds = %259, %235
  %238 = phi i64 [ 0, %235 ], [ %260, %259 ]
  %239 = phi i32 [ %233, %235 ], [ %258, %259 ]
  %240 = icmp slt i64 %238, 196
  br i1 %240, label %241, label %261

241:                                              ; preds = %237
  %242 = mul i64 %238, 8
  %243 = getelementptr i8, ptr %236, i64 %242
  %244 = load ptr, ptr %243, align 8
  %245 = load double, ptr %244, align 8
  %246 = getelementptr [196 x double], ptr %3, i64 %232, i64 %238
  %247 = load double, ptr %246, align 8
  %248 = fsub double %245, %247
  %249 = call double @llvm.fabs.f64(double %248)
  %250 = fcmp ogt double %249, 0x3EB0C6F7A0B5ED8D
  br i1 %250, label %251, label %256

251:                                              ; preds = %241
  %252 = trunc i64 %238 to i32
  %253 = trunc i64 %232 to i32
  %254 = add i32 %239, 1
  %255 = call i32 (ptr, ...) @printf(ptr @str1, i32 %253, i32 %252, double %245, double %247)
  br label %257

256:                                              ; preds = %241
  br label %257

257:                                              ; preds = %251, %256
  %258 = phi i32 [ %239, %256 ], [ %254, %251 ]
  br label %259

259:                                              ; preds = %257
  %260 = add i64 %238, 1
  br label %237

261:                                              ; preds = %237
  %262 = add i64 %232, 1
  br label %231

263:                                              ; preds = %231
  %264 = icmp eq i32 %233, 0
  br i1 %264, label %265, label %267

265:                                              ; preds = %263
  %266 = call i32 (ptr, ...) @printf(ptr @str2)
  br label %269

267:                                              ; preds = %263
  %268 = call i32 (ptr, ...) @printf(ptr @str3)
  br label %269

269:                                              ; preds = %265, %267
  %270 = icmp sgt i32 %233, 0
  %271 = zext i1 %270 to i32
  ret i32 %271
}

declare double @omp_get_wtime()

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = alloca i64, i64 1, align 8
  store i64 0, ptr %6, align 8
  %7 = alloca [196 x i64], i64 196, align 8
  br label %8

8:                                                ; preds = %25, %4
  %9 = phi i64 [ 0, %4 ], [ %26, %25 ]
  %10 = icmp slt i64 %9, 196
  br i1 %10, label %11, label %27

11:                                               ; preds = %8
  br label %12

12:                                               ; preds = %15, %11
  %13 = phi i64 [ 0, %11 ], [ %24, %15 ]
  %14 = icmp slt i64 %13, 196
  br i1 %14, label %15, label %25

15:                                               ; preds = %12
  %16 = load i64, ptr %6, align 8
  %17 = mul i64 %16, 24
  %18 = trunc i64 %17 to i32
  %19 = getelementptr i8, ptr %3, i32 %18
  %20 = getelementptr { i64, i32, ptr }, ptr %19, i32 0, i32 0
  %21 = load i64, ptr %20, align 8
  %22 = getelementptr [196 x i64], ptr %7, i64 %9, i64 %13
  store i64 %21, ptr %22, align 8
  %23 = add i64 %16, 1
  store i64 %23, ptr %6, align 8
  %24 = add i64 %13, 1
  br label %12

25:                                               ; preds = %12
  %26 = add i64 %9, 1
  br label %8

27:                                               ; preds = %8
  %28 = alloca [5 x i64], i64 5, align 8
  br label %29

29:                                               ; preds = %46, %27
  %30 = phi i64 [ 0, %27 ], [ %47, %46 ]
  %31 = icmp slt i64 %30, 5
  br i1 %31, label %32, label %48

32:                                               ; preds = %29
  br label %33

33:                                               ; preds = %36, %32
  %34 = phi i64 [ 0, %32 ], [ %45, %36 ]
  %35 = icmp slt i64 %34, 5
  br i1 %35, label %36, label %46

36:                                               ; preds = %33
  %37 = load i64, ptr %6, align 8
  %38 = mul i64 %37, 24
  %39 = trunc i64 %38 to i32
  %40 = getelementptr i8, ptr %3, i32 %39
  %41 = getelementptr { i64, i32, ptr }, ptr %40, i32 0, i32 0
  %42 = load i64, ptr %41, align 8
  %43 = getelementptr [5 x i64], ptr %28, i64 %30, i64 %34
  store i64 %42, ptr %43, align 8
  %44 = add i64 %37, 1
  store i64 %44, ptr %6, align 8
  %45 = add i64 %34, 1
  br label %33

46:                                               ; preds = %33
  %47 = add i64 %30, 1
  br label %29

48:                                               ; preds = %29
  %49 = alloca [200 x i64], i64 200, align 8
  br label %50

50:                                               ; preds = %67, %48
  %51 = phi i64 [ 0, %48 ], [ %68, %67 ]
  %52 = icmp slt i64 %51, 200
  br i1 %52, label %53, label %69

53:                                               ; preds = %50
  br label %54

54:                                               ; preds = %57, %53
  %55 = phi i64 [ 0, %53 ], [ %66, %57 ]
  %56 = icmp slt i64 %55, 200
  br i1 %56, label %57, label %67

57:                                               ; preds = %54
  %58 = load i64, ptr %6, align 8
  %59 = mul i64 %58, 24
  %60 = trunc i64 %59 to i32
  %61 = getelementptr i8, ptr %3, i32 %60
  %62 = getelementptr { i64, i32, ptr }, ptr %61, i32 0, i32 0
  %63 = load i64, ptr %62, align 8
  %64 = getelementptr [200 x i64], ptr %49, i64 %51, i64 %55
  store i64 %63, ptr %64, align 8
  %65 = add i64 %58, 1
  store i64 %65, ptr %6, align 8
  %66 = add i64 %55, 1
  br label %54

67:                                               ; preds = %54
  %68 = add i64 %51, 1
  br label %50

69:                                               ; preds = %50
  %70 = alloca i64, i64 2, align 8
  br label %71

71:                                               ; preds = %154, %69
  %72 = phi i64 [ 0, %69 ], [ %155, %154 ]
  %73 = icmp slt i64 %72, 4
  br i1 %73, label %74, label %156

74:                                               ; preds = %71
  %75 = trunc i64 %72 to i32
  %76 = mul i32 %75, 50
  %77 = add i32 %76, 50
  %78 = call i64 @artsGetCurrentEpochGuid()
  %79 = call i32 @artsGetCurrentNode()
  %80 = sext i32 %77 to i64
  store i64 %80, ptr %70, align 8
  %81 = sext i32 %76 to i64
  %82 = getelementptr i64, ptr %70, i32 1
  store i64 %81, ptr %82, align 8
  %83 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %79, i32 2, ptr %70, i32 78441, i64 %78)
  %84 = call ptr @malloc(i64 8)
  store i64 0, ptr %84, align 8
  br label %85

85:                                               ; preds = %99, %74
  %86 = phi i64 [ 0, %74 ], [ %100, %99 ]
  %87 = icmp slt i64 %86, 196
  br i1 %87, label %88, label %101

88:                                               ; preds = %85
  br label %89

89:                                               ; preds = %92, %88
  %90 = phi i64 [ 0, %88 ], [ %98, %92 ]
  %91 = icmp slt i64 %90, 196
  br i1 %91, label %92, label %99

92:                                               ; preds = %89
  %93 = load i64, ptr %84, align 8
  %94 = getelementptr [196 x i64], ptr %7, i64 %86, i64 %90
  %95 = load i64, ptr %94, align 8
  %96 = trunc i64 %93 to i32
  call void @artsDbAddDependence(i64 %95, i64 %83, i32 %96)
  %97 = add i64 %93, 1
  store i64 %97, ptr %84, align 8
  %98 = add i64 %90, 1
  br label %89

99:                                               ; preds = %89
  %100 = add i64 %86, 1
  br label %85

101:                                              ; preds = %85
  %102 = call ptr @malloc(i64 8)
  store i64 38416, ptr %102, align 8
  br label %103

103:                                              ; preds = %117, %101
  %104 = phi i64 [ 0, %101 ], [ %118, %117 ]
  %105 = icmp slt i64 %104, 5
  br i1 %105, label %106, label %119

106:                                              ; preds = %103
  br label %107

107:                                              ; preds = %110, %106
  %108 = phi i64 [ 0, %106 ], [ %116, %110 ]
  %109 = icmp slt i64 %108, 5
  br i1 %109, label %110, label %117

110:                                              ; preds = %107
  %111 = load i64, ptr %102, align 8
  %112 = getelementptr [5 x i64], ptr %28, i64 %104, i64 %108
  %113 = load i64, ptr %112, align 8
  %114 = trunc i64 %111 to i32
  call void @artsDbAddDependence(i64 %113, i64 %83, i32 %114)
  %115 = add i64 %111, 1
  store i64 %115, ptr %102, align 8
  %116 = add i64 %108, 1
  br label %107

117:                                              ; preds = %107
  %118 = add i64 %104, 1
  br label %103

119:                                              ; preds = %103
  %120 = call ptr @malloc(i64 8)
  store i64 38441, ptr %120, align 8
  br label %121

121:                                              ; preds = %135, %119
  %122 = phi i64 [ 0, %119 ], [ %136, %135 ]
  %123 = icmp slt i64 %122, 200
  br i1 %123, label %124, label %137

124:                                              ; preds = %121
  br label %125

125:                                              ; preds = %128, %124
  %126 = phi i64 [ 0, %124 ], [ %134, %128 ]
  %127 = icmp slt i64 %126, 200
  br i1 %127, label %128, label %135

128:                                              ; preds = %125
  %129 = load i64, ptr %120, align 8
  %130 = getelementptr [200 x i64], ptr %49, i64 %122, i64 %126
  %131 = load i64, ptr %130, align 8
  %132 = trunc i64 %129 to i32
  call void @artsDbAddDependence(i64 %131, i64 %83, i32 %132)
  %133 = add i64 %129, 1
  store i64 %133, ptr %120, align 8
  %134 = add i64 %126, 1
  br label %125

135:                                              ; preds = %125
  %136 = add i64 %122, 1
  br label %121

137:                                              ; preds = %121
  %138 = call ptr @malloc(i64 8)
  store i64 0, ptr %138, align 8
  br label %139

139:                                              ; preds = %152, %137
  %140 = phi i64 [ 0, %137 ], [ %153, %152 ]
  %141 = icmp slt i64 %140, 196
  br i1 %141, label %142, label %154

142:                                              ; preds = %139
  br label %143

143:                                              ; preds = %146, %142
  %144 = phi i64 [ 0, %142 ], [ %151, %146 ]
  %145 = icmp slt i64 %144, 196
  br i1 %145, label %146, label %152

146:                                              ; preds = %143
  %147 = getelementptr [196 x i64], ptr %7, i64 %140, i64 %144
  %148 = load i64, ptr %147, align 8
  call void @artsDbIncrementLatch(i64 %148)
  %149 = load i64, ptr %138, align 8
  %150 = add i64 %149, 1
  store i64 %150, ptr %138, align 8
  %151 = add i64 %144, 1
  br label %143

152:                                              ; preds = %143
  %153 = add i64 %140, 1
  br label %139

154:                                              ; preds = %139
  %155 = add i64 %72, 1
  br label %71

156:                                              ; preds = %71
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = trunc i64 %6 to i32
  %8 = getelementptr i64, ptr %1, i32 1
  %9 = load i64, ptr %8, align 8
  %10 = trunc i64 %9 to i32
  %11 = alloca i64, i64 1, align 8
  store i64 0, ptr %11, align 8
  %12 = alloca [196 x ptr], i64 196, align 8
  br label %13

13:                                               ; preds = %30, %4
  %14 = phi i64 [ 0, %4 ], [ %31, %30 ]
  %15 = icmp slt i64 %14, 196
  br i1 %15, label %16, label %32

16:                                               ; preds = %13
  br label %17

17:                                               ; preds = %20, %16
  %18 = phi i64 [ 0, %16 ], [ %29, %20 ]
  %19 = icmp slt i64 %18, 196
  br i1 %19, label %20, label %30

20:                                               ; preds = %17
  %21 = load i64, ptr %11, align 8
  %22 = mul i64 %21, 24
  %23 = trunc i64 %22 to i32
  %24 = getelementptr i8, ptr %3, i32 %23
  %25 = getelementptr { i64, i32, ptr }, ptr %24, i32 0, i32 2
  %26 = load ptr, ptr %25, align 8
  %27 = getelementptr [196 x ptr], ptr %12, i64 %14, i64 %18
  store ptr %26, ptr %27, align 8
  %28 = add i64 %21, 1
  store i64 %28, ptr %11, align 8
  %29 = add i64 %18, 1
  br label %17

30:                                               ; preds = %17
  %31 = add i64 %14, 1
  br label %13

32:                                               ; preds = %13
  %33 = alloca [5 x ptr], i64 5, align 8
  br label %34

34:                                               ; preds = %51, %32
  %35 = phi i64 [ 0, %32 ], [ %52, %51 ]
  %36 = icmp slt i64 %35, 5
  br i1 %36, label %37, label %53

37:                                               ; preds = %34
  br label %38

38:                                               ; preds = %41, %37
  %39 = phi i64 [ 0, %37 ], [ %50, %41 ]
  %40 = icmp slt i64 %39, 5
  br i1 %40, label %41, label %51

41:                                               ; preds = %38
  %42 = load i64, ptr %11, align 8
  %43 = mul i64 %42, 24
  %44 = trunc i64 %43 to i32
  %45 = getelementptr i8, ptr %3, i32 %44
  %46 = getelementptr { i64, i32, ptr }, ptr %45, i32 0, i32 2
  %47 = load ptr, ptr %46, align 8
  %48 = getelementptr [5 x ptr], ptr %33, i64 %35, i64 %39
  store ptr %47, ptr %48, align 8
  %49 = add i64 %42, 1
  store i64 %49, ptr %11, align 8
  %50 = add i64 %39, 1
  br label %38

51:                                               ; preds = %38
  %52 = add i64 %35, 1
  br label %34

53:                                               ; preds = %34
  %54 = alloca [200 x ptr], i64 200, align 8
  br label %55

55:                                               ; preds = %72, %53
  %56 = phi i64 [ 0, %53 ], [ %73, %72 ]
  %57 = icmp slt i64 %56, 200
  br i1 %57, label %58, label %74

58:                                               ; preds = %55
  br label %59

59:                                               ; preds = %62, %58
  %60 = phi i64 [ 0, %58 ], [ %71, %62 ]
  %61 = icmp slt i64 %60, 200
  br i1 %61, label %62, label %72

62:                                               ; preds = %59
  %63 = load i64, ptr %11, align 8
  %64 = mul i64 %63, 24
  %65 = trunc i64 %64 to i32
  %66 = getelementptr i8, ptr %3, i32 %65
  %67 = getelementptr { i64, i32, ptr }, ptr %66, i32 0, i32 2
  %68 = load ptr, ptr %67, align 8
  %69 = getelementptr [200 x ptr], ptr %54, i64 %56, i64 %60
  store ptr %68, ptr %69, align 8
  %70 = add i64 %63, 1
  store i64 %70, ptr %11, align 8
  %71 = add i64 %60, 1
  br label %59

72:                                               ; preds = %59
  %73 = add i64 %56, 1
  br label %55

74:                                               ; preds = %55
  br label %75

75:                                               ; preds = %133, %74
  %76 = phi i32 [ %134, %133 ], [ %10, %74 ]
  %77 = icmp slt i32 %76, %7
  %78 = icmp slt i32 %76, 196
  %79 = and i1 %77, %78
  br i1 %79, label %80, label %135

80:                                               ; preds = %75
  %81 = phi i32 [ %76, %75 ]
  %82 = sext i32 %81 to i64
  %83 = getelementptr [196 x ptr], ptr %12, i64 %82, i32 0
  br label %84

84:                                               ; preds = %128, %80
  %85 = phi i64 [ 0, %80 ], [ %132, %128 ]
  %86 = icmp slt i64 %85, 196
  br i1 %86, label %87, label %133

87:                                               ; preds = %84
  %88 = trunc i64 %85 to i32
  br label %89

89:                                               ; preds = %126, %87
  %90 = phi i64 [ 0, %87 ], [ %127, %126 ]
  %91 = phi double [ 0.000000e+00, %87 ], [ %99, %126 ]
  %92 = icmp slt i64 %90, 5
  br i1 %92, label %93, label %128

93:                                               ; preds = %89
  %94 = trunc i64 %90 to i32
  %95 = add i32 %81, %94
  %96 = icmp slt i32 %95, 200
  br label %97

97:                                               ; preds = %124, %93
  %98 = phi i64 [ 0, %93 ], [ %125, %124 ]
  %99 = phi double [ %91, %93 ], [ %123, %124 ]
  %100 = icmp slt i64 %98, 5
  br i1 %100, label %101, label %126

101:                                              ; preds = %97
  %102 = trunc i64 %98 to i32
  %103 = add i32 %88, %102
  %104 = icmp slt i32 %103, 200
  %105 = and i1 %96, %104
  br i1 %105, label %106, label %121

106:                                              ; preds = %101
  %107 = sext i32 %95 to i64
  %108 = sext i32 %103 to i64
  %109 = getelementptr [200 x ptr], ptr %54, i64 %107, i32 0
  %110 = mul i64 %108, 8
  %111 = getelementptr i8, ptr %109, i64 %110
  %112 = load ptr, ptr %111, align 8
  %113 = load double, ptr %112, align 8
  %114 = getelementptr [5 x ptr], ptr %33, i64 %90, i32 0
  %115 = mul i64 %98, 8
  %116 = getelementptr i8, ptr %114, i64 %115
  %117 = load ptr, ptr %116, align 8
  %118 = load double, ptr %117, align 8
  %119 = fmul double %113, %118
  %120 = fadd double %99, %119
  br label %122

121:                                              ; preds = %101
  br label %122

122:                                              ; preds = %106, %121
  %123 = phi double [ %99, %121 ], [ %120, %106 ]
  br label %124

124:                                              ; preds = %122
  %125 = add i64 %98, 1
  br label %97

126:                                              ; preds = %97
  %127 = add i64 %90, 1
  br label %89

128:                                              ; preds = %89
  %129 = mul i64 %85, 8
  %130 = getelementptr i8, ptr %83, i64 %129
  %131 = load ptr, ptr %130, align 8
  store double %91, ptr %131, align 8
  %132 = add i64 %85, 1
  br label %84

133:                                              ; preds = %84
  %134 = add i32 %81, 1
  br label %75

135:                                              ; preds = %75
  ret void
}

define void @artsMain(i32 %0, ptr %1) {
  %3 = call i32 @mainBody(i32 %0, ptr %1)
  call void @artsShutdown()
  ret void
}

define i32 @main(i32 %0, ptr %1) {
  %3 = call i32 @artsRT(i32 %0, ptr %1)
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare double @llvm.fabs.f64(double) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}

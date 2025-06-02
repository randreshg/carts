; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str8 = internal constant [19 x i8] c"Result: INCORRECT\0A\00"
@str7 = internal constant [17 x i8] c"Result: CORRECT\0A\00"
@str6 = internal constant [16 x i8] c"Speedup: %.2fx\0A\00"
@str5 = internal constant [45 x i8] c"Parallel time: %.4fs\0ASequential time: %.4fs\0A\00"
@str4 = internal constant [43 x i8] c"Sequential Cholesky %s! (Max error: %.2e)\0A\00"
@str3 = internal constant [20 x i8] c"verification FAILED\00"
@str2 = internal constant [20 x i8] c"verification passed\00"
@str1 = internal constant [41 x i8] c"Parallel Cholesky %s! (Max error: %.2e)\0A\00"
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

define i32 @mainBody() {
  %1 = alloca double, i64 16384, align 8
  %2 = alloca double, i64 16384, align 8
  %3 = call i32 @artsGetCurrentNode()
  %4 = alloca i64, i64 16384, align 8
  %5 = alloca ptr, i64 16384, align 8
  br label %6

6:                                                ; preds = %9, %0
  %7 = phi i64 [ 0, %0 ], [ %14, %9 ]
  %8 = icmp slt i64 %7, 16384
  br i1 %8, label %9, label %15

9:                                                ; preds = %6
  %10 = call i64 @artsReserveGuidRoute(i32 10, i32 %3)
  %11 = call ptr @artsDbCreateWithGuid(i64 %10, i64 8)
  %12 = getelementptr i64, ptr %4, i64 %7
  store i64 %10, ptr %12, align 8
  %13 = getelementptr ptr, ptr %5, i64 %7
  store ptr %11, ptr %13, align 8
  %14 = add i64 %7, 1
  br label %6

15:                                               ; preds = %6
  %16 = alloca double, i64 16384, align 8
  %17 = call i64 @time(ptr null)
  %18 = trunc i64 %17 to i32
  call void @srand(i32 %18)
  br label %19

19:                                               ; preds = %46, %15
  %20 = phi i64 [ 0, %15 ], [ %53, %46 ]
  %21 = icmp slt i64 %20, 128
  br i1 %21, label %22, label %54

22:                                               ; preds = %19
  %23 = trunc i64 %20 to i32
  br label %24

24:                                               ; preds = %44, %22
  %25 = phi i64 [ 0, %22 ], [ %45, %44 ]
  %26 = icmp slt i64 %25, 128
  br i1 %26, label %27, label %46

27:                                               ; preds = %24
  %28 = trunc i64 %25 to i32
  %29 = icmp sle i32 %28, %23
  br i1 %29, label %30, label %39

30:                                               ; preds = %27
  %31 = mul i32 %23, 128
  %32 = add i32 %31, %28
  %33 = sext i32 %32 to i64
  %34 = call i32 @rand()
  %35 = sitofp i32 %34 to double
  %36 = fdiv double %35, 0x41DFFFFFFFC00000
  %37 = fadd double %36, 1.000000e-02
  %38 = getelementptr double, ptr %1, i64 %33
  store double %37, ptr %38, align 8
  br label %44

39:                                               ; preds = %27
  %40 = mul i32 %23, 128
  %41 = add i32 %40, %28
  %42 = sext i32 %41 to i64
  %43 = getelementptr double, ptr %1, i64 %42
  store double 0.000000e+00, ptr %43, align 8
  br label %44

44:                                               ; preds = %30, %39
  %45 = add i64 %25, 1
  br label %24

46:                                               ; preds = %24
  %47 = mul i32 %23, 128
  %48 = add i32 %47, %23
  %49 = sext i32 %48 to i64
  %50 = getelementptr double, ptr %1, i64 %49
  %51 = load double, ptr %50, align 8
  %52 = fadd double %51, 1.000000e+00
  store double %52, ptr %50, align 8
  %53 = add i64 %20, 1
  br label %19

54:                                               ; preds = %19
  br label %55

55:                                               ; preds = %93, %54
  %56 = phi i64 [ 0, %54 ], [ %94, %93 ]
  %57 = icmp slt i64 %56, 128
  br i1 %57, label %58, label %95

58:                                               ; preds = %55
  %59 = trunc i64 %56 to i32
  %60 = add i32 %59, 1
  %61 = sext i32 %60 to i64
  %62 = mul i32 %59, 128
  br label %63

63:                                               ; preds = %91, %58
  %64 = phi i64 [ 0, %58 ], [ %92, %91 ]
  %65 = icmp slt i64 %64, %61
  br i1 %65, label %66, label %93

66:                                               ; preds = %63
  %67 = trunc i64 %64 to i32
  %68 = add i32 %62, %67
  %69 = sext i32 %68 to i64
  %70 = getelementptr double, ptr %16, i64 %69
  store double 0.000000e+00, ptr %70, align 8
  %71 = add i32 %67, 1
  %72 = sext i32 %71 to i64
  %73 = mul i32 %67, 128
  br label %74

74:                                               ; preds = %78, %66
  %75 = phi i64 [ 0, %66 ], [ %90, %78 ]
  %76 = phi double [ 0.000000e+00, %66 ], [ %89, %78 ]
  %77 = icmp slt i64 %75, %72
  br i1 %77, label %78, label %91

78:                                               ; preds = %74
  %79 = trunc i64 %75 to i32
  %80 = add i32 %62, %79
  %81 = sext i32 %80 to i64
  %82 = getelementptr double, ptr %1, i64 %81
  %83 = load double, ptr %82, align 8
  %84 = add i32 %73, %79
  %85 = sext i32 %84 to i64
  %86 = getelementptr double, ptr %1, i64 %85
  %87 = load double, ptr %86, align 8
  %88 = fmul double %83, %87
  %89 = fadd double %76, %88
  store double %89, ptr %70, align 8
  %90 = add i64 %75, 1
  br label %74

91:                                               ; preds = %74
  %92 = add i64 %64, 1
  br label %63

93:                                               ; preds = %63
  %94 = add i64 %56, 1
  br label %55

95:                                               ; preds = %55
  br label %96

96:                                               ; preds = %118, %95
  %97 = phi i64 [ 0, %95 ], [ %119, %118 ]
  %98 = icmp slt i64 %97, 128
  br i1 %98, label %99, label %120

99:                                               ; preds = %96
  %100 = trunc i64 %97 to i32
  %101 = add i32 %100, 1
  %102 = sext i32 %101 to i64
  %103 = mul i32 %100, 128
  br label %104

104:                                              ; preds = %107, %99
  %105 = phi i64 [ %102, %99 ], [ %117, %107 ]
  %106 = icmp slt i64 %105, 128
  br i1 %106, label %107, label %118

107:                                              ; preds = %104
  %108 = trunc i64 %105 to i32
  %109 = add i32 %103, %108
  %110 = sext i32 %109 to i64
  %111 = mul i32 %108, 128
  %112 = add i32 %111, %100
  %113 = sext i32 %112 to i64
  %114 = getelementptr double, ptr %16, i64 %113
  %115 = load double, ptr %114, align 8
  %116 = getelementptr double, ptr %16, i64 %110
  store double %115, ptr %116, align 8
  %117 = add i64 %105, 1
  br label %104

118:                                              ; preds = %104
  %119 = add i64 %97, 1
  br label %96

120:                                              ; preds = %96
  br label %121

121:                                              ; preds = %141, %120
  %122 = phi i64 [ 0, %120 ], [ %142, %141 ]
  %123 = icmp slt i64 %122, 128
  br i1 %123, label %124, label %143

124:                                              ; preds = %121
  %125 = trunc i64 %122 to i32
  %126 = mul i32 %125, 128
  br label %127

127:                                              ; preds = %130, %124
  %128 = phi i64 [ 0, %124 ], [ %140, %130 ]
  %129 = icmp slt i64 %128, 128
  br i1 %129, label %130, label %141

130:                                              ; preds = %127
  %131 = trunc i64 %128 to i32
  %132 = add i32 %126, %131
  %133 = sext i32 %132 to i64
  %134 = getelementptr double, ptr %16, i64 %133
  %135 = load double, ptr %134, align 8
  %136 = mul i64 %133, 8
  %137 = getelementptr i8, ptr %5, i64 %136
  %138 = load ptr, ptr %137, align 8
  store double %135, ptr %138, align 8
  %139 = getelementptr double, ptr %2, i64 %133
  store double %135, ptr %139, align 8
  %140 = add i64 %128, 1
  br label %127

141:                                              ; preds = %127
  %142 = add i64 %122, 1
  br label %121

143:                                              ; preds = %121
  %144 = call double @omp_get_wtime()
  %145 = call i32 @artsGetCurrentNode()
  %146 = alloca i64, i64 0, align 8
  %147 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %145, i32 0, ptr %146, i32 1)
  %148 = call i64 @artsInitializeAndStartEpoch(i64 %147, i32 0)
  %149 = call i32 @artsGetCurrentNode()
  %150 = alloca i64, i64 2, align 8
  store i64 undef, ptr %150, align 8
  %151 = getelementptr i64, ptr %150, i32 1
  store i64 undef, ptr %151, align 8
  %152 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %149, i32 2, ptr %150, i32 16384, i64 %148)
  %153 = call ptr @malloc(i64 8)
  store i64 0, ptr %153, align 8
  br label %154

154:                                              ; preds = %157, %143
  %155 = phi i64 [ 0, %143 ], [ %163, %157 ]
  %156 = icmp slt i64 %155, 16384
  br i1 %156, label %157, label %164

157:                                              ; preds = %154
  %158 = load i64, ptr %153, align 8
  %159 = getelementptr i64, ptr %4, i64 %155
  %160 = load i64, ptr %159, align 8
  %161 = trunc i64 %158 to i32
  call void @artsDbAddDependence(i64 %160, i64 %152, i32 %161)
  %162 = add i64 %158, 1
  store i64 %162, ptr %153, align 8
  %163 = add i64 %155, 1
  br label %154

164:                                              ; preds = %154
  %165 = call ptr @malloc(i64 8)
  store i64 0, ptr %165, align 8
  br label %166

166:                                              ; preds = %169, %164
  %167 = phi i64 [ 0, %164 ], [ %174, %169 ]
  %168 = icmp slt i64 %167, 16384
  br i1 %168, label %169, label %175

169:                                              ; preds = %166
  %170 = getelementptr i64, ptr %4, i64 %167
  %171 = load i64, ptr %170, align 8
  call void @artsDbIncrementLatch(i64 %171)
  %172 = load i64, ptr %165, align 8
  %173 = add i64 %172, 1
  store i64 %173, ptr %165, align 8
  %174 = add i64 %167, 1
  br label %166

175:                                              ; preds = %166
  %176 = call i1 @artsWaitOnHandle(i64 %148)
  %177 = call double @omp_get_wtime()
  %178 = fsub double %177, %144
  %179 = call i32 (ptr, ...) @printf(ptr @str0, double %178)
  %180 = call double @omp_get_wtime()
  br label %181

181:                                              ; preds = %259, %175
  %182 = phi i64 [ 0, %175 ], [ %260, %259 ]
  %183 = icmp slt i64 %182, 128
  br i1 %183, label %184, label %261

184:                                              ; preds = %181
  %185 = trunc i64 %182 to i32
  %186 = mul i32 %185, 128
  %187 = add i32 %186, %185
  %188 = sext i32 %187 to i64
  %189 = getelementptr double, ptr %2, i64 %188
  %190 = load double, ptr %189, align 8
  %191 = fcmp olt double %190, 0.000000e+00
  %192 = select i1 %191, double 0.000000e+00, double %190
  br i1 %191, label %193, label %194

193:                                              ; preds = %184
  store double 0.000000e+00, ptr %189, align 8
  br label %194

194:                                              ; preds = %193, %184
  %195 = call double @llvm.sqrt.f64(double %192)
  store double %195, ptr %189, align 8
  %196 = fcmp une double %195, 0.000000e+00
  br i1 %196, label %197, label %214

197:                                              ; preds = %194
  %198 = add i32 %185, 1
  %199 = sext i32 %198 to i64
  br label %200

200:                                              ; preds = %203, %197
  %201 = phi i64 [ %199, %197 ], [ %212, %203 ]
  %202 = icmp slt i64 %201, 128
  br i1 %202, label %203, label %213

203:                                              ; preds = %200
  %204 = trunc i64 %201 to i32
  %205 = mul i32 %204, 128
  %206 = add i32 %205, %185
  %207 = sext i32 %206 to i64
  %208 = load double, ptr %189, align 8
  %209 = getelementptr double, ptr %2, i64 %207
  %210 = load double, ptr %209, align 8
  %211 = fdiv double %210, %208
  store double %211, ptr %209, align 8
  %212 = add i64 %201, 1
  br label %200

213:                                              ; preds = %200, %217
  br label %227

214:                                              ; preds = %194
  %215 = add i32 %185, 1
  %216 = sext i32 %215 to i64
  br label %217

217:                                              ; preds = %220, %214
  %218 = phi i64 [ %216, %214 ], [ %226, %220 ]
  %219 = icmp slt i64 %218, 128
  br i1 %219, label %220, label %213

220:                                              ; preds = %217
  %221 = trunc i64 %218 to i32
  %222 = mul i32 %221, 128
  %223 = add i32 %222, %185
  %224 = sext i32 %223 to i64
  %225 = getelementptr double, ptr %2, i64 %224
  store double 0.000000e+00, ptr %225, align 8
  %226 = add i64 %218, 1
  br label %217

227:                                              ; preds = %213
  %228 = add i32 %185, 1
  %229 = sext i32 %228 to i64
  br label %230

230:                                              ; preds = %257, %227
  %231 = phi i64 [ %229, %227 ], [ %258, %257 ]
  %232 = icmp slt i64 %231, 128
  br i1 %232, label %233, label %259

233:                                              ; preds = %230
  %234 = trunc i64 %231 to i32
  %235 = mul i32 %234, 128
  %236 = add i32 %235, %185
  %237 = sext i32 %236 to i64
  br label %238

238:                                              ; preds = %241, %233
  %239 = phi i64 [ %231, %233 ], [ %256, %241 ]
  %240 = icmp slt i64 %239, 128
  br i1 %240, label %241, label %257

241:                                              ; preds = %238
  %242 = trunc i64 %239 to i32
  %243 = mul i32 %242, 128
  %244 = add i32 %243, %234
  %245 = sext i32 %244 to i64
  %246 = add i32 %243, %185
  %247 = sext i32 %246 to i64
  %248 = getelementptr double, ptr %2, i64 %247
  %249 = load double, ptr %248, align 8
  %250 = getelementptr double, ptr %2, i64 %237
  %251 = load double, ptr %250, align 8
  %252 = fmul double %249, %251
  %253 = getelementptr double, ptr %2, i64 %245
  %254 = load double, ptr %253, align 8
  %255 = fsub double %254, %252
  store double %255, ptr %253, align 8
  %256 = add i64 %239, 1
  br label %238

257:                                              ; preds = %238
  %258 = add i64 %231, 1
  br label %230

259:                                              ; preds = %230
  %260 = add i64 %182, 1
  br label %181

261:                                              ; preds = %181
  %262 = call double @omp_get_wtime()
  %263 = fsub double %262, %180
  br label %264

264:                                              ; preds = %325, %261
  %265 = phi i64 [ 0, %261 ], [ %326, %325 ]
  %266 = phi double [ 0.000000e+00, %261 ], [ %276, %325 ]
  %267 = phi i32 [ 1, %261 ], [ %277, %325 ]
  %268 = icmp slt i64 %265, 128
  br i1 %268, label %269, label %327

269:                                              ; preds = %264
  %270 = trunc i64 %265 to i32
  %271 = add i32 %270, 1
  %272 = sext i32 %271 to i64
  %273 = mul i32 %270, 128
  br label %274

274:                                              ; preds = %323, %269
  %275 = phi i64 [ 0, %269 ], [ %324, %323 ]
  %276 = phi double [ %266, %269 ], [ %313, %323 ]
  %277 = phi i32 [ %267, %269 ], [ %322, %323 ]
  %278 = icmp slt i64 %275, %272
  br i1 %278, label %279, label %325

279:                                              ; preds = %274
  %280 = trunc i64 %275 to i32
  %281 = add i32 %280, 1
  %282 = sext i32 %281 to i64
  %283 = mul i32 %280, 128
  br label %284

284:                                              ; preds = %288, %279
  %285 = phi i64 [ 0, %279 ], [ %304, %288 ]
  %286 = phi double [ 0.000000e+00, %279 ], [ %303, %288 ]
  %287 = icmp slt i64 %285, %282
  br i1 %287, label %288, label %305

288:                                              ; preds = %284
  %289 = trunc i64 %285 to i32
  %290 = add i32 %273, %289
  %291 = sext i32 %290 to i64
  %292 = mul i64 %291, 8
  %293 = getelementptr i8, ptr %5, i64 %292
  %294 = load ptr, ptr %293, align 8
  %295 = load double, ptr %294, align 8
  %296 = add i32 %283, %289
  %297 = sext i32 %296 to i64
  %298 = mul i64 %297, 8
  %299 = getelementptr i8, ptr %5, i64 %298
  %300 = load ptr, ptr %299, align 8
  %301 = load double, ptr %300, align 8
  %302 = fmul double %295, %301
  %303 = fadd double %286, %302
  %304 = add i64 %285, 1
  br label %284

305:                                              ; preds = %284
  %306 = add i32 %273, %280
  %307 = sext i32 %306 to i64
  %308 = getelementptr double, ptr %16, i64 %307
  %309 = load double, ptr %308, align 8
  %310 = fsub double %309, %286
  %311 = call double @llvm.fabs.f64(double %310)
  %312 = fcmp ogt double %311, %276
  %313 = select i1 %312, double %311, double %276
  %314 = call double @llvm.fabs.f64(double %309)
  %315 = fmul double %314, 0x3EB0C6F7A0B5ED8D
  %316 = fcmp ogt double %311, %315
  br i1 %316, label %317, label %320

317:                                              ; preds = %305
  %318 = fcmp ogt double %311, 0x3EB0C6F7A0B5ED8D
  %319 = select i1 %318, i32 0, i32 %277
  br label %321

320:                                              ; preds = %305
  br label %321

321:                                              ; preds = %317, %320
  %322 = phi i32 [ %277, %320 ], [ %319, %317 ]
  br label %323

323:                                              ; preds = %321
  %324 = add i64 %275, 1
  br label %274

325:                                              ; preds = %274
  %326 = add i64 %265, 1
  br label %264

327:                                              ; preds = %264
  br label %328

328:                                              ; preds = %385, %327
  %329 = phi i64 [ 0, %327 ], [ %386, %385 ]
  %330 = phi double [ 0.000000e+00, %327 ], [ %340, %385 ]
  %331 = phi i32 [ 1, %327 ], [ %341, %385 ]
  %332 = icmp slt i64 %329, 128
  br i1 %332, label %333, label %387

333:                                              ; preds = %328
  %334 = trunc i64 %329 to i32
  %335 = add i32 %334, 1
  %336 = sext i32 %335 to i64
  %337 = mul i32 %334, 128
  br label %338

338:                                              ; preds = %383, %333
  %339 = phi i64 [ 0, %333 ], [ %384, %383 ]
  %340 = phi double [ %330, %333 ], [ %373, %383 ]
  %341 = phi i32 [ %331, %333 ], [ %382, %383 ]
  %342 = icmp slt i64 %339, %336
  br i1 %342, label %343, label %385

343:                                              ; preds = %338
  %344 = trunc i64 %339 to i32
  %345 = add i32 %344, 1
  %346 = sext i32 %345 to i64
  %347 = mul i32 %344, 128
  br label %348

348:                                              ; preds = %352, %343
  %349 = phi i64 [ 0, %343 ], [ %364, %352 ]
  %350 = phi double [ 0.000000e+00, %343 ], [ %363, %352 ]
  %351 = icmp slt i64 %349, %346
  br i1 %351, label %352, label %365

352:                                              ; preds = %348
  %353 = trunc i64 %349 to i32
  %354 = add i32 %337, %353
  %355 = sext i32 %354 to i64
  %356 = getelementptr double, ptr %2, i64 %355
  %357 = load double, ptr %356, align 8
  %358 = add i32 %347, %353
  %359 = sext i32 %358 to i64
  %360 = getelementptr double, ptr %2, i64 %359
  %361 = load double, ptr %360, align 8
  %362 = fmul double %357, %361
  %363 = fadd double %350, %362
  %364 = add i64 %349, 1
  br label %348

365:                                              ; preds = %348
  %366 = add i32 %337, %344
  %367 = sext i32 %366 to i64
  %368 = getelementptr double, ptr %16, i64 %367
  %369 = load double, ptr %368, align 8
  %370 = fsub double %369, %350
  %371 = call double @llvm.fabs.f64(double %370)
  %372 = fcmp ogt double %371, %340
  %373 = select i1 %372, double %371, double %340
  %374 = call double @llvm.fabs.f64(double %369)
  %375 = fmul double %374, 0x3EB0C6F7A0B5ED8D
  %376 = fcmp ogt double %371, %375
  br i1 %376, label %377, label %380

377:                                              ; preds = %365
  %378 = fcmp ogt double %371, 0x3EB0C6F7A0B5ED8D
  %379 = select i1 %378, i32 0, i32 %341
  br label %381

380:                                              ; preds = %365
  br label %381

381:                                              ; preds = %377, %380
  %382 = phi i32 [ %341, %380 ], [ %379, %377 ]
  br label %383

383:                                              ; preds = %381
  %384 = add i64 %339, 1
  br label %338

385:                                              ; preds = %338
  %386 = add i64 %329, 1
  br label %328

387:                                              ; preds = %328
  %388 = icmp ne i32 %331, 0
  %389 = icmp ne i32 %267, 0
  br i1 %389, label %390, label %391

390:                                              ; preds = %387
  br label %392

391:                                              ; preds = %387
  br label %392

392:                                              ; preds = %390, %391
  %393 = phi ptr [ @str3, %391 ], [ @str2, %390 ]
  br label %394

394:                                              ; preds = %392
  %395 = call i32 (ptr, ...) @printf(ptr @str1, ptr %393, double %266)
  br i1 %388, label %396, label %397

396:                                              ; preds = %394
  br label %398

397:                                              ; preds = %394
  br label %398

398:                                              ; preds = %396, %397
  %399 = phi ptr [ @str3, %397 ], [ @str2, %396 ]
  br label %400

400:                                              ; preds = %398
  %401 = call i32 (ptr, ...) @printf(ptr @str4, ptr %399, double %330)
  %402 = call i32 (ptr, ...) @printf(ptr @str5, double %178, double %263)
  %403 = fcmp ogt double %263, 1.000000e-09
  %404 = fcmp ogt double %178, 1.000000e-09
  %405 = and i1 %403, %404
  br i1 %405, label %406, label %409

406:                                              ; preds = %400
  %407 = fdiv double %263, %178
  %408 = call i32 (ptr, ...) @printf(ptr @str6, double %407)
  br label %409

409:                                              ; preds = %406, %400
  br i1 %389, label %410, label %412

410:                                              ; preds = %409
  %411 = call i32 (ptr, ...) @printf(ptr @str7)
  br label %414

412:                                              ; preds = %409
  %413 = call i32 (ptr, ...) @printf(ptr @str8)
  br label %414

414:                                              ; preds = %410, %412
  ret i32 0
}

declare void @srand(i32)

declare i64 @time(ptr)

declare i32 @rand()

declare double @omp_get_wtime()

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = trunc i64 %6 to i32
  %8 = getelementptr i64, ptr %1, i32 1
  %9 = load i64, ptr %8, align 8
  %10 = sitofp i64 %9 to double
  %11 = alloca i64, i64 1, align 8
  store i64 0, ptr %11, align 8
  %12 = alloca i64, i64 16384, align 8
  br label %13

13:                                               ; preds = %16, %4
  %14 = phi i64 [ 0, %4 ], [ %25, %16 ]
  %15 = icmp slt i64 %14, 16384
  br i1 %15, label %16, label %26

16:                                               ; preds = %13
  %17 = load i64, ptr %11, align 8
  %18 = mul i64 %17, 24
  %19 = trunc i64 %18 to i32
  %20 = getelementptr i8, ptr %3, i32 %19
  %21 = getelementptr { i64, i32, ptr }, ptr %20, i32 0, i32 0
  %22 = load i64, ptr %21, align 8
  %23 = getelementptr i64, ptr %12, i64 %14
  store i64 %22, ptr %23, align 8
  %24 = add i64 %17, 1
  store i64 %24, ptr %11, align 8
  %25 = add i64 %14, 1
  br label %13

26:                                               ; preds = %13
  %27 = alloca i64, i64 2, align 8
  %28 = alloca i64, i64 1, align 8
  %29 = alloca i64, i64 1, align 8
  %30 = alloca i64, i64 1, align 8
  %31 = alloca i64, i64 1, align 8
  %32 = sext i32 %7 to i64
  %33 = fptosi double %10 to i64
  br label %34

34:                                               ; preds = %207, %26
  %35 = phi i64 [ 0, %26 ], [ %208, %207 ]
  %36 = icmp slt i64 %35, 128
  br i1 %36, label %37, label %209

37:                                               ; preds = %34
  %38 = trunc i64 %35 to i32
  %39 = add i32 %38, 16
  %40 = icmp sgt i32 %39, 128
  br i1 %40, label %41, label %43

41:                                               ; preds = %37
  %42 = sub i32 128, %38
  br label %44

43:                                               ; preds = %37
  br label %44

44:                                               ; preds = %41, %43
  %45 = phi i32 [ 16, %43 ], [ %42, %41 ]
  br label %46

46:                                               ; preds = %44
  %47 = add i32 %38, %45
  %48 = sext i32 %47 to i64
  %49 = call i64 @artsGetCurrentEpochGuid()
  %50 = call i32 @artsGetCurrentNode()
  store i64 %35, ptr %27, align 8
  %51 = getelementptr i64, ptr %27, i32 1
  store i64 %48, ptr %51, align 8
  %52 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %50, i32 2, ptr %27, i32 16384, i64 %49)
  %53 = call ptr @malloc(i64 8)
  store i64 0, ptr %53, align 8
  br label %54

54:                                               ; preds = %57, %46
  %55 = phi i64 [ 0, %46 ], [ %63, %57 ]
  %56 = icmp slt i64 %55, 16384
  br i1 %56, label %57, label %64

57:                                               ; preds = %54
  %58 = load i64, ptr %53, align 8
  %59 = getelementptr i64, ptr %12, i64 %55
  %60 = load i64, ptr %59, align 8
  %61 = trunc i64 %58 to i32
  call void @artsDbAddDependence(i64 %60, i64 %52, i32 %61)
  %62 = add i64 %58, 1
  store i64 %62, ptr %53, align 8
  %63 = add i64 %55, 1
  br label %54

64:                                               ; preds = %54
  %65 = call ptr @malloc(i64 8)
  store i64 0, ptr %65, align 8
  br label %66

66:                                               ; preds = %69, %64
  %67 = phi i64 [ 0, %64 ], [ %74, %69 ]
  %68 = icmp slt i64 %67, 16384
  br i1 %68, label %69, label %75

69:                                               ; preds = %66
  %70 = getelementptr i64, ptr %12, i64 %67
  %71 = load i64, ptr %70, align 8
  call void @artsDbIncrementLatch(i64 %71)
  %72 = load i64, ptr %65, align 8
  %73 = add i64 %72, 1
  store i64 %73, ptr %65, align 8
  %74 = add i64 %67, 1
  br label %66

75:                                               ; preds = %66
  br label %76

76:                                               ; preds = %126, %75
  %77 = phi i64 [ %48, %75 ], [ %127, %126 ]
  %78 = icmp slt i64 %77, 128
  br i1 %78, label %79, label %128

79:                                               ; preds = %76
  %80 = trunc i64 %77 to i32
  %81 = add i32 %80, 16
  %82 = icmp sgt i32 %81, 128
  br i1 %82, label %83, label %85

83:                                               ; preds = %79
  %84 = sub i32 128, %80
  br label %86

85:                                               ; preds = %79
  br label %86

86:                                               ; preds = %83, %85
  %87 = phi i32 [ 16, %85 ], [ %84, %83 ]
  br label %88

88:                                               ; preds = %86
  %89 = add i32 %80, %87
  %90 = sext i32 %89 to i64
  %91 = call i64 @artsGetCurrentEpochGuid()
  %92 = call i32 @artsGetCurrentNode()
  store i64 0, ptr %28, align 8
  %93 = load i64, ptr %28, align 8
  %94 = add i64 %93, 16384
  store i64 %94, ptr %28, align 8
  %95 = load i64, ptr %28, align 8
  %96 = trunc i64 %95 to i32
  store i64 4, ptr %29, align 8
  %97 = load i64, ptr %29, align 8
  %98 = trunc i64 %97 to i32
  %99 = alloca i64, i64 %97, align 8
  store i64 %35, ptr %99, align 8
  %100 = getelementptr i64, ptr %99, i32 1
  store i64 %77, ptr %100, align 8
  %101 = getelementptr i64, ptr %99, i32 2
  store i64 %90, ptr %101, align 8
  %102 = getelementptr i64, ptr %99, i32 3
  store i64 %48, ptr %102, align 8
  %103 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_4, i32 %92, i32 %98, ptr %99, i32 %96, i64 %91)
  %104 = call ptr @malloc(i64 8)
  store i64 %93, ptr %104, align 8
  br label %105

105:                                              ; preds = %108, %88
  %106 = phi i64 [ 0, %88 ], [ %114, %108 ]
  %107 = icmp slt i64 %106, 16384
  br i1 %107, label %108, label %115

108:                                              ; preds = %105
  %109 = load i64, ptr %104, align 8
  %110 = getelementptr i64, ptr %12, i64 %106
  %111 = load i64, ptr %110, align 8
  %112 = trunc i64 %109 to i32
  call void @artsDbAddDependence(i64 %111, i64 %103, i32 %112)
  %113 = add i64 %109, 1
  store i64 %113, ptr %104, align 8
  %114 = add i64 %106, 1
  br label %105

115:                                              ; preds = %105
  %116 = call ptr @malloc(i64 8)
  store i64 %93, ptr %116, align 8
  br label %117

117:                                              ; preds = %120, %115
  %118 = phi i64 [ 0, %115 ], [ %125, %120 ]
  %119 = icmp slt i64 %118, 16384
  br i1 %119, label %120, label %126

120:                                              ; preds = %117
  %121 = getelementptr i64, ptr %12, i64 %118
  %122 = load i64, ptr %121, align 8
  call void @artsDbIncrementLatch(i64 %122)
  %123 = load i64, ptr %116, align 8
  %124 = add i64 %123, 1
  store i64 %124, ptr %116, align 8
  %125 = add i64 %118, 1
  br label %117

126:                                              ; preds = %117
  %127 = add i64 %77, 16
  br label %76

128:                                              ; preds = %76
  br label %129

129:                                              ; preds = %206, %128
  %130 = phi i32 [ %137, %206 ], [ %38, %128 ]
  %131 = phi i32 [ 16, %206 ], [ %45, %128 ]
  %132 = add i32 %130, %131
  %133 = icmp slt i32 %132, 128
  br i1 %133, label %134, label %207

134:                                              ; preds = %129
  %135 = phi i32 [ %130, %129 ]
  %136 = phi i32 [ %131, %129 ]
  %137 = add i32 %135, %136
  %138 = add i32 %137, 16
  %139 = icmp sgt i32 %138, 128
  br i1 %139, label %140, label %142

140:                                              ; preds = %134
  %141 = sub i32 128, %137
  br label %143

142:                                              ; preds = %134
  br label %143

143:                                              ; preds = %140, %142
  %144 = phi i32 [ 16, %142 ], [ %141, %140 ]
  br label %145

145:                                              ; preds = %143
  %146 = sext i32 %137 to i64
  %147 = add i32 %137, %144
  %148 = sext i32 %147 to i64
  br label %149

149:                                              ; preds = %204, %145
  %150 = phi i64 [ %146, %145 ], [ %205, %204 ]
  %151 = icmp slt i64 %150, 128
  br i1 %151, label %152, label %206

152:                                              ; preds = %149
  %153 = trunc i64 %150 to i32
  %154 = add i32 %153, 16
  %155 = icmp sgt i32 %154, 128
  br i1 %155, label %156, label %158

156:                                              ; preds = %152
  %157 = sub i32 128, %153
  br label %159

158:                                              ; preds = %152
  br label %159

159:                                              ; preds = %156, %158
  %160 = phi i32 [ 16, %158 ], [ %157, %156 ]
  br label %161

161:                                              ; preds = %159
  %162 = add i32 %153, %160
  %163 = sext i32 %162 to i64
  %164 = call i64 @artsGetCurrentEpochGuid()
  %165 = call i32 @artsGetCurrentNode()
  store i64 0, ptr %30, align 8
  %166 = load i64, ptr %30, align 8
  %167 = add i64 %166, 16384
  store i64 %167, ptr %30, align 8
  %168 = load i64, ptr %30, align 8
  %169 = trunc i64 %168 to i32
  store i64 9, ptr %31, align 8
  %170 = load i64, ptr %31, align 8
  %171 = trunc i64 %170 to i32
  %172 = alloca i64, i64 %170, align 8
  store i64 %48, ptr %172, align 8
  %173 = getelementptr i64, ptr %172, i32 1
  store i64 %35, ptr %173, align 8
  %174 = getelementptr i64, ptr %172, i32 2
  store i64 %48, ptr %174, align 8
  %175 = getelementptr i64, ptr %172, i32 3
  store i64 %146, ptr %175, align 8
  %176 = getelementptr i64, ptr %172, i32 4
  store i64 %148, ptr %176, align 8
  %177 = getelementptr i64, ptr %172, i32 5
  store i64 %150, ptr %177, align 8
  %178 = getelementptr i64, ptr %172, i32 6
  store i64 %163, ptr %178, align 8
  %179 = getelementptr i64, ptr %172, i32 7
  store i64 %32, ptr %179, align 8
  %180 = getelementptr i64, ptr %172, i32 8
  store i64 %33, ptr %180, align 8
  %181 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_5, i32 %165, i32 %171, ptr %172, i32 %169, i64 %164)
  %182 = call ptr @malloc(i64 8)
  store i64 %166, ptr %182, align 8
  br label %183

183:                                              ; preds = %186, %161
  %184 = phi i64 [ 0, %161 ], [ %192, %186 ]
  %185 = icmp slt i64 %184, 16384
  br i1 %185, label %186, label %193

186:                                              ; preds = %183
  %187 = load i64, ptr %182, align 8
  %188 = getelementptr i64, ptr %12, i64 %184
  %189 = load i64, ptr %188, align 8
  %190 = trunc i64 %187 to i32
  call void @artsDbAddDependence(i64 %189, i64 %181, i32 %190)
  %191 = add i64 %187, 1
  store i64 %191, ptr %182, align 8
  %192 = add i64 %184, 1
  br label %183

193:                                              ; preds = %183
  %194 = call ptr @malloc(i64 8)
  store i64 %166, ptr %194, align 8
  br label %195

195:                                              ; preds = %198, %193
  %196 = phi i64 [ 0, %193 ], [ %203, %198 ]
  %197 = icmp slt i64 %196, 16384
  br i1 %197, label %198, label %204

198:                                              ; preds = %195
  %199 = getelementptr i64, ptr %12, i64 %196
  %200 = load i64, ptr %199, align 8
  call void @artsDbIncrementLatch(i64 %200)
  %201 = load i64, ptr %194, align 8
  %202 = add i64 %201, 1
  store i64 %202, ptr %194, align 8
  %203 = add i64 %196, 1
  br label %195

204:                                              ; preds = %195
  %205 = add i64 %150, 16
  br label %149

206:                                              ; preds = %149
  br label %129

207:                                              ; preds = %129
  %208 = add i64 %35, 16
  br label %34

209:                                              ; preds = %34
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = getelementptr i64, ptr %1, i32 1
  %8 = load i64, ptr %7, align 8
  %9 = alloca i64, i64 1, align 8
  store i64 0, ptr %9, align 8
  %10 = alloca ptr, i64 16384, align 8
  br label %11

11:                                               ; preds = %14, %4
  %12 = phi i64 [ 0, %4 ], [ %23, %14 ]
  %13 = icmp slt i64 %12, 16384
  br i1 %13, label %14, label %24

14:                                               ; preds = %11
  %15 = load i64, ptr %9, align 8
  %16 = mul i64 %15, 24
  %17 = trunc i64 %16 to i32
  %18 = getelementptr i8, ptr %3, i32 %17
  %19 = getelementptr { i64, i32, ptr }, ptr %18, i32 0, i32 2
  %20 = load ptr, ptr %19, align 8
  %21 = getelementptr ptr, ptr %10, i64 %12
  store ptr %20, ptr %21, align 8
  %22 = add i64 %15, 1
  store i64 %22, ptr %9, align 8
  %23 = add i64 %12, 1
  br label %11

24:                                               ; preds = %11
  br label %25

25:                                               ; preds = %107, %24
  %26 = phi i64 [ %6, %24 ], [ %108, %107 ]
  %27 = icmp slt i64 %26, %8
  br i1 %27, label %28, label %109

28:                                               ; preds = %25
  %29 = trunc i64 %26 to i32
  %30 = mul i32 %29, 128
  %31 = add i32 %30, %29
  %32 = sext i32 %31 to i64
  %33 = mul i64 %32, 8
  %34 = getelementptr i8, ptr %10, i64 %33
  br label %35

35:                                               ; preds = %38, %28
  %36 = phi i64 [ %6, %28 ], [ %50, %38 ]
  %37 = icmp slt i64 %36, %26
  br i1 %37, label %38, label %51

38:                                               ; preds = %35
  %39 = trunc i64 %36 to i32
  %40 = add i32 %30, %39
  %41 = sext i32 %40 to i64
  %42 = mul i64 %41, 8
  %43 = getelementptr i8, ptr %10, i64 %42
  %44 = load ptr, ptr %43, align 8
  %45 = load double, ptr %44, align 8
  %46 = fmul double %45, %45
  %47 = load ptr, ptr %34, align 8
  %48 = load double, ptr %47, align 8
  %49 = fsub double %48, %46
  store double %49, ptr %47, align 8
  %50 = add i64 %36, 1
  br label %35

51:                                               ; preds = %35
  %52 = load ptr, ptr %34, align 8
  %53 = load double, ptr %52, align 8
  %54 = fcmp olt double %53, 0.000000e+00
  br i1 %54, label %55, label %57

55:                                               ; preds = %51
  %56 = load ptr, ptr %34, align 8
  store double 0.000000e+00, ptr %56, align 8
  br label %57

57:                                               ; preds = %55, %51
  %58 = load ptr, ptr %34, align 8
  %59 = load double, ptr %58, align 8
  %60 = call double @llvm.sqrt.f64(double %59)
  store double %60, ptr %58, align 8
  %61 = add i32 %29, 1
  %62 = sext i32 %61 to i64
  br label %63

63:                                               ; preds = %105, %57
  %64 = phi i64 [ %62, %57 ], [ %106, %105 ]
  %65 = icmp slt i64 %64, %8
  br i1 %65, label %66, label %107

66:                                               ; preds = %63
  %67 = trunc i64 %64 to i32
  %68 = mul i32 %67, 128
  %69 = add i32 %68, %29
  %70 = sext i32 %69 to i64
  %71 = mul i64 %70, 8
  %72 = getelementptr i8, ptr %10, i64 %71
  br label %73

73:                                               ; preds = %76, %66
  %74 = phi i64 [ %6, %66 ], [ %94, %76 ]
  %75 = icmp slt i64 %74, %26
  br i1 %75, label %76, label %95

76:                                               ; preds = %73
  %77 = trunc i64 %74 to i32
  %78 = add i32 %68, %77
  %79 = sext i32 %78 to i64
  %80 = mul i64 %79, 8
  %81 = getelementptr i8, ptr %10, i64 %80
  %82 = load ptr, ptr %81, align 8
  %83 = load double, ptr %82, align 8
  %84 = add i32 %30, %77
  %85 = sext i32 %84 to i64
  %86 = mul i64 %85, 8
  %87 = getelementptr i8, ptr %10, i64 %86
  %88 = load ptr, ptr %87, align 8
  %89 = load double, ptr %88, align 8
  %90 = fmul double %83, %89
  %91 = load ptr, ptr %72, align 8
  %92 = load double, ptr %91, align 8
  %93 = fsub double %92, %90
  store double %93, ptr %91, align 8
  %94 = add i64 %74, 1
  br label %73

95:                                               ; preds = %73
  %96 = load ptr, ptr %34, align 8
  %97 = load double, ptr %96, align 8
  %98 = fcmp une double %97, 0.000000e+00
  br i1 %98, label %99, label %103

99:                                               ; preds = %95
  %100 = load ptr, ptr %72, align 8
  %101 = load double, ptr %100, align 8
  %102 = fdiv double %101, %97
  store double %102, ptr %100, align 8
  br label %105

103:                                              ; preds = %95
  %104 = load ptr, ptr %72, align 8
  store double 0.000000e+00, ptr %104, align 8
  br label %105

105:                                              ; preds = %99, %103
  %106 = add i64 %64, 1
  br label %63

107:                                              ; preds = %63
  %108 = add i64 %26, 1
  br label %25

109:                                              ; preds = %25
  ret void
}

define void @__arts_edt_4(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = getelementptr i64, ptr %1, i32 1
  %8 = load i64, ptr %7, align 8
  %9 = getelementptr i64, ptr %1, i32 2
  %10 = load i64, ptr %9, align 8
  %11 = getelementptr i64, ptr %1, i32 3
  %12 = load i64, ptr %11, align 8
  %13 = alloca i64, i64 1, align 8
  store i64 0, ptr %13, align 8
  %14 = alloca ptr, i64 16384, align 8
  br label %15

15:                                               ; preds = %18, %4
  %16 = phi i64 [ 0, %4 ], [ %27, %18 ]
  %17 = icmp slt i64 %16, 16384
  br i1 %17, label %18, label %28

18:                                               ; preds = %15
  %19 = load i64, ptr %13, align 8
  %20 = mul i64 %19, 24
  %21 = trunc i64 %20 to i32
  %22 = getelementptr i8, ptr %3, i32 %21
  %23 = getelementptr { i64, i32, ptr }, ptr %22, i32 0, i32 2
  %24 = load ptr, ptr %23, align 8
  %25 = getelementptr ptr, ptr %14, i64 %16
  store ptr %24, ptr %25, align 8
  %26 = add i64 %19, 1
  store i64 %26, ptr %13, align 8
  %27 = add i64 %16, 1
  br label %15

28:                                               ; preds = %15
  br label %29

29:                                               ; preds = %83, %28
  %30 = phi i64 [ %6, %28 ], [ %84, %83 ]
  %31 = icmp slt i64 %30, %12
  br i1 %31, label %32, label %85

32:                                               ; preds = %29
  %33 = trunc i64 %30 to i32
  %34 = mul i32 %33, 128
  %35 = add i32 %34, %33
  %36 = sext i32 %35 to i64
  %37 = mul i64 %36, 8
  %38 = getelementptr i8, ptr %14, i64 %37
  br label %39

39:                                               ; preds = %81, %32
  %40 = phi i64 [ %8, %32 ], [ %82, %81 ]
  %41 = icmp slt i64 %40, %10
  br i1 %41, label %42, label %83

42:                                               ; preds = %39
  %43 = trunc i64 %40 to i32
  %44 = mul i32 %43, 128
  %45 = add i32 %44, %33
  %46 = sext i32 %45 to i64
  %47 = mul i64 %46, 8
  %48 = getelementptr i8, ptr %14, i64 %47
  br label %49

49:                                               ; preds = %52, %42
  %50 = phi i64 [ %6, %42 ], [ %70, %52 ]
  %51 = icmp slt i64 %50, %30
  br i1 %51, label %52, label %71

52:                                               ; preds = %49
  %53 = trunc i64 %50 to i32
  %54 = add i32 %44, %53
  %55 = sext i32 %54 to i64
  %56 = mul i64 %55, 8
  %57 = getelementptr i8, ptr %14, i64 %56
  %58 = load ptr, ptr %57, align 8
  %59 = load double, ptr %58, align 8
  %60 = add i32 %34, %53
  %61 = sext i32 %60 to i64
  %62 = mul i64 %61, 8
  %63 = getelementptr i8, ptr %14, i64 %62
  %64 = load ptr, ptr %63, align 8
  %65 = load double, ptr %64, align 8
  %66 = fmul double %59, %65
  %67 = load ptr, ptr %48, align 8
  %68 = load double, ptr %67, align 8
  %69 = fsub double %68, %66
  store double %69, ptr %67, align 8
  %70 = add i64 %50, 1
  br label %49

71:                                               ; preds = %49
  %72 = load ptr, ptr %38, align 8
  %73 = load double, ptr %72, align 8
  %74 = fcmp une double %73, 0.000000e+00
  br i1 %74, label %75, label %79

75:                                               ; preds = %71
  %76 = load ptr, ptr %48, align 8
  %77 = load double, ptr %76, align 8
  %78 = fdiv double %77, %73
  store double %78, ptr %76, align 8
  br label %81

79:                                               ; preds = %71
  %80 = load ptr, ptr %48, align 8
  store double 0.000000e+00, ptr %80, align 8
  br label %81

81:                                               ; preds = %75, %79
  %82 = add i64 %40, 1
  br label %39

83:                                               ; preds = %39
  %84 = add i64 %30, 1
  br label %29

85:                                               ; preds = %29
  ret void
}

define void @__arts_edt_5(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = getelementptr i64, ptr %1, i32 1
  %7 = load i64, ptr %6, align 8
  %8 = getelementptr i64, ptr %1, i32 2
  %9 = load i64, ptr %8, align 8
  %10 = getelementptr i64, ptr %1, i32 3
  %11 = load i64, ptr %10, align 8
  %12 = getelementptr i64, ptr %1, i32 4
  %13 = load i64, ptr %12, align 8
  %14 = getelementptr i64, ptr %1, i32 5
  %15 = load i64, ptr %14, align 8
  %16 = getelementptr i64, ptr %1, i32 6
  %17 = load i64, ptr %16, align 8
  %18 = alloca i64, i64 1, align 8
  store i64 0, ptr %18, align 8
  %19 = alloca ptr, i64 16384, align 8
  br label %20

20:                                               ; preds = %23, %4
  %21 = phi i64 [ 0, %4 ], [ %32, %23 ]
  %22 = icmp slt i64 %21, 16384
  br i1 %22, label %23, label %33

23:                                               ; preds = %20
  %24 = load i64, ptr %18, align 8
  %25 = mul i64 %24, 24
  %26 = trunc i64 %25 to i32
  %27 = getelementptr i8, ptr %3, i32 %26
  %28 = getelementptr { i64, i32, ptr }, ptr %27, i32 0, i32 2
  %29 = load ptr, ptr %28, align 8
  %30 = getelementptr ptr, ptr %19, i64 %21
  store ptr %29, ptr %30, align 8
  %31 = add i64 %24, 1
  store i64 %31, ptr %18, align 8
  %32 = add i64 %21, 1
  br label %20

33:                                               ; preds = %20
  br label %34

34:                                               ; preds = %81, %33
  %35 = phi i64 [ %15, %33 ], [ %82, %81 ]
  %36 = icmp slt i64 %35, %17
  br i1 %36, label %37, label %83

37:                                               ; preds = %34
  %38 = trunc i64 %35 to i32
  br label %39

39:                                               ; preds = %79, %37
  %40 = phi i64 [ %11, %37 ], [ %80, %79 ]
  %41 = icmp slt i64 %40, %13
  br i1 %41, label %42, label %81

42:                                               ; preds = %39
  %43 = trunc i64 %40 to i32
  %44 = icmp sge i32 %38, %43
  br i1 %44, label %45, label %77

45:                                               ; preds = %42
  %46 = mul i32 %38, 128
  %47 = mul i32 %43, 128
  br label %48

48:                                               ; preds = %52, %45
  %49 = phi i64 [ %7, %45 ], [ %68, %52 ]
  %50 = phi double [ 0.000000e+00, %45 ], [ %67, %52 ]
  %51 = icmp slt i64 %49, %9
  br i1 %51, label %52, label %69

52:                                               ; preds = %48
  %53 = trunc i64 %49 to i32
  %54 = add i32 %46, %53
  %55 = sext i32 %54 to i64
  %56 = mul i64 %55, 8
  %57 = getelementptr i8, ptr %19, i64 %56
  %58 = load ptr, ptr %57, align 8
  %59 = load double, ptr %58, align 8
  %60 = add i32 %47, %53
  %61 = sext i32 %60 to i64
  %62 = mul i64 %61, 8
  %63 = getelementptr i8, ptr %19, i64 %62
  %64 = load ptr, ptr %63, align 8
  %65 = load double, ptr %64, align 8
  %66 = fmul double %59, %65
  %67 = fadd double %50, %66
  %68 = add i64 %49, 1
  br label %48

69:                                               ; preds = %48
  %70 = add i32 %46, %43
  %71 = sext i32 %70 to i64
  %72 = mul i64 %71, 8
  %73 = getelementptr i8, ptr %19, i64 %72
  %74 = load ptr, ptr %73, align 8
  %75 = load double, ptr %74, align 8
  %76 = fsub double %75, %50
  store double %76, ptr %74, align 8
  br label %78

77:                                               ; preds = %42
  br label %78

78:                                               ; preds = %69, %77
  br label %79

79:                                               ; preds = %78
  %80 = add i64 %40, 1
  br label %39

81:                                               ; preds = %39
  %82 = add i64 %35, 1
  br label %34

83:                                               ; preds = %34
  ret void
}

define void @artsMain(i32 %0, ptr %1) {
  %3 = call i32 @mainBody()
  call void @artsShutdown()
  ret void
}

define i32 @main(i32 %0, ptr %1) {
  %3 = call i32 @artsRT(i32 %0, ptr %1)
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare double @llvm.fabs.f64(double) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare double @llvm.sqrt.f64(double) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}

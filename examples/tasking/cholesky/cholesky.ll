; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str5 = internal constant [16 x i8] c"Speedup: %.2fx\0A\00"
@str4 = internal constant [45 x i8] c"Parallel time: %.4fs\0ASequential time: %.4fs\0A\00"
@str3 = internal constant [43 x i8] c"Sequential Cholesky %s! (Max error: %.2e)\0A\00"
@str2 = internal constant [20 x i8] c"verification FAILED\00"
@str1 = internal constant [20 x i8] c"verification passed\00"
@str0 = internal constant [41 x i8] c"Parallel Cholesky %s! (Max error: %.2e)\0A\00"

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

declare void @artsShutdown()

declare i64 @artsGetCurrentEpochGuid()

declare void @artsDbIncrementLatch(i64)

declare void @artsDbAddDependence(i64, i64, i32)

declare void @artsDbDecrementLatch(i64)

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
  %179 = call double @omp_get_wtime()
  br label %180

180:                                              ; preds = %258, %175
  %181 = phi i64 [ 0, %175 ], [ %259, %258 ]
  %182 = icmp slt i64 %181, 128
  br i1 %182, label %183, label %260

183:                                              ; preds = %180
  %184 = trunc i64 %181 to i32
  %185 = mul i32 %184, 128
  %186 = add i32 %185, %184
  %187 = sext i32 %186 to i64
  %188 = getelementptr double, ptr %2, i64 %187
  %189 = load double, ptr %188, align 8
  %190 = fcmp olt double %189, 0.000000e+00
  %191 = select i1 %190, double 0.000000e+00, double %189
  br i1 %190, label %192, label %193

192:                                              ; preds = %183
  store double 0.000000e+00, ptr %188, align 8
  br label %193

193:                                              ; preds = %192, %183
  %194 = call double @llvm.sqrt.f64(double %191)
  store double %194, ptr %188, align 8
  %195 = fcmp une double %194, 0.000000e+00
  br i1 %195, label %196, label %213

196:                                              ; preds = %193
  %197 = add i32 %184, 1
  %198 = sext i32 %197 to i64
  br label %199

199:                                              ; preds = %202, %196
  %200 = phi i64 [ %198, %196 ], [ %211, %202 ]
  %201 = icmp slt i64 %200, 128
  br i1 %201, label %202, label %212

202:                                              ; preds = %199
  %203 = trunc i64 %200 to i32
  %204 = mul i32 %203, 128
  %205 = add i32 %204, %184
  %206 = sext i32 %205 to i64
  %207 = load double, ptr %188, align 8
  %208 = getelementptr double, ptr %2, i64 %206
  %209 = load double, ptr %208, align 8
  %210 = fdiv double %209, %207
  store double %210, ptr %208, align 8
  %211 = add i64 %200, 1
  br label %199

212:                                              ; preds = %199, %216
  br label %226

213:                                              ; preds = %193
  %214 = add i32 %184, 1
  %215 = sext i32 %214 to i64
  br label %216

216:                                              ; preds = %219, %213
  %217 = phi i64 [ %215, %213 ], [ %225, %219 ]
  %218 = icmp slt i64 %217, 128
  br i1 %218, label %219, label %212

219:                                              ; preds = %216
  %220 = trunc i64 %217 to i32
  %221 = mul i32 %220, 128
  %222 = add i32 %221, %184
  %223 = sext i32 %222 to i64
  %224 = getelementptr double, ptr %2, i64 %223
  store double 0.000000e+00, ptr %224, align 8
  %225 = add i64 %217, 1
  br label %216

226:                                              ; preds = %212
  %227 = add i32 %184, 1
  %228 = sext i32 %227 to i64
  br label %229

229:                                              ; preds = %256, %226
  %230 = phi i64 [ %228, %226 ], [ %257, %256 ]
  %231 = icmp slt i64 %230, 128
  br i1 %231, label %232, label %258

232:                                              ; preds = %229
  %233 = trunc i64 %230 to i32
  %234 = mul i32 %233, 128
  %235 = add i32 %234, %184
  %236 = sext i32 %235 to i64
  br label %237

237:                                              ; preds = %240, %232
  %238 = phi i64 [ %230, %232 ], [ %255, %240 ]
  %239 = icmp slt i64 %238, 128
  br i1 %239, label %240, label %256

240:                                              ; preds = %237
  %241 = trunc i64 %238 to i32
  %242 = mul i32 %241, 128
  %243 = add i32 %242, %233
  %244 = sext i32 %243 to i64
  %245 = add i32 %242, %184
  %246 = sext i32 %245 to i64
  %247 = getelementptr double, ptr %2, i64 %246
  %248 = load double, ptr %247, align 8
  %249 = getelementptr double, ptr %2, i64 %236
  %250 = load double, ptr %249, align 8
  %251 = fmul double %248, %250
  %252 = getelementptr double, ptr %2, i64 %244
  %253 = load double, ptr %252, align 8
  %254 = fsub double %253, %251
  store double %254, ptr %252, align 8
  %255 = add i64 %238, 1
  br label %237

256:                                              ; preds = %237
  %257 = add i64 %230, 1
  br label %229

258:                                              ; preds = %229
  %259 = add i64 %181, 1
  br label %180

260:                                              ; preds = %180
  %261 = call double @omp_get_wtime()
  %262 = fsub double %261, %179
  br label %263

263:                                              ; preds = %324, %260
  %264 = phi i64 [ 0, %260 ], [ %325, %324 ]
  %265 = phi double [ 0.000000e+00, %260 ], [ %275, %324 ]
  %266 = phi i32 [ 1, %260 ], [ %276, %324 ]
  %267 = icmp slt i64 %264, 128
  br i1 %267, label %268, label %326

268:                                              ; preds = %263
  %269 = trunc i64 %264 to i32
  %270 = add i32 %269, 1
  %271 = sext i32 %270 to i64
  %272 = mul i32 %269, 128
  br label %273

273:                                              ; preds = %322, %268
  %274 = phi i64 [ 0, %268 ], [ %323, %322 ]
  %275 = phi double [ %265, %268 ], [ %312, %322 ]
  %276 = phi i32 [ %266, %268 ], [ %321, %322 ]
  %277 = icmp slt i64 %274, %271
  br i1 %277, label %278, label %324

278:                                              ; preds = %273
  %279 = trunc i64 %274 to i32
  %280 = add i32 %279, 1
  %281 = sext i32 %280 to i64
  %282 = mul i32 %279, 128
  br label %283

283:                                              ; preds = %287, %278
  %284 = phi i64 [ 0, %278 ], [ %303, %287 ]
  %285 = phi double [ 0.000000e+00, %278 ], [ %302, %287 ]
  %286 = icmp slt i64 %284, %281
  br i1 %286, label %287, label %304

287:                                              ; preds = %283
  %288 = trunc i64 %284 to i32
  %289 = add i32 %272, %288
  %290 = sext i32 %289 to i64
  %291 = mul i64 %290, 8
  %292 = getelementptr i8, ptr %5, i64 %291
  %293 = load ptr, ptr %292, align 8
  %294 = load double, ptr %293, align 8
  %295 = add i32 %282, %288
  %296 = sext i32 %295 to i64
  %297 = mul i64 %296, 8
  %298 = getelementptr i8, ptr %5, i64 %297
  %299 = load ptr, ptr %298, align 8
  %300 = load double, ptr %299, align 8
  %301 = fmul double %294, %300
  %302 = fadd double %285, %301
  %303 = add i64 %284, 1
  br label %283

304:                                              ; preds = %283
  %305 = add i32 %272, %279
  %306 = sext i32 %305 to i64
  %307 = getelementptr double, ptr %16, i64 %306
  %308 = load double, ptr %307, align 8
  %309 = fsub double %308, %285
  %310 = call double @llvm.fabs.f64(double %309)
  %311 = fcmp ogt double %310, %275
  %312 = select i1 %311, double %310, double %275
  %313 = call double @llvm.fabs.f64(double %308)
  %314 = fmul double %313, 0x3EB0C6F7A0B5ED8D
  %315 = fcmp ogt double %310, %314
  br i1 %315, label %316, label %319

316:                                              ; preds = %304
  %317 = fcmp ogt double %310, 0x3EB0C6F7A0B5ED8D
  %318 = select i1 %317, i32 0, i32 %276
  br label %320

319:                                              ; preds = %304
  br label %320

320:                                              ; preds = %316, %319
  %321 = phi i32 [ %276, %319 ], [ %318, %316 ]
  br label %322

322:                                              ; preds = %320
  %323 = add i64 %274, 1
  br label %273

324:                                              ; preds = %273
  %325 = add i64 %264, 1
  br label %263

326:                                              ; preds = %263
  br label %327

327:                                              ; preds = %384, %326
  %328 = phi i64 [ 0, %326 ], [ %385, %384 ]
  %329 = phi double [ 0.000000e+00, %326 ], [ %339, %384 ]
  %330 = phi i32 [ 1, %326 ], [ %340, %384 ]
  %331 = icmp slt i64 %328, 128
  br i1 %331, label %332, label %386

332:                                              ; preds = %327
  %333 = trunc i64 %328 to i32
  %334 = add i32 %333, 1
  %335 = sext i32 %334 to i64
  %336 = mul i32 %333, 128
  br label %337

337:                                              ; preds = %382, %332
  %338 = phi i64 [ 0, %332 ], [ %383, %382 ]
  %339 = phi double [ %329, %332 ], [ %372, %382 ]
  %340 = phi i32 [ %330, %332 ], [ %381, %382 ]
  %341 = icmp slt i64 %338, %335
  br i1 %341, label %342, label %384

342:                                              ; preds = %337
  %343 = trunc i64 %338 to i32
  %344 = add i32 %343, 1
  %345 = sext i32 %344 to i64
  %346 = mul i32 %343, 128
  br label %347

347:                                              ; preds = %351, %342
  %348 = phi i64 [ 0, %342 ], [ %363, %351 ]
  %349 = phi double [ 0.000000e+00, %342 ], [ %362, %351 ]
  %350 = icmp slt i64 %348, %345
  br i1 %350, label %351, label %364

351:                                              ; preds = %347
  %352 = trunc i64 %348 to i32
  %353 = add i32 %336, %352
  %354 = sext i32 %353 to i64
  %355 = getelementptr double, ptr %2, i64 %354
  %356 = load double, ptr %355, align 8
  %357 = add i32 %346, %352
  %358 = sext i32 %357 to i64
  %359 = getelementptr double, ptr %2, i64 %358
  %360 = load double, ptr %359, align 8
  %361 = fmul double %356, %360
  %362 = fadd double %349, %361
  %363 = add i64 %348, 1
  br label %347

364:                                              ; preds = %347
  %365 = add i32 %336, %343
  %366 = sext i32 %365 to i64
  %367 = getelementptr double, ptr %16, i64 %366
  %368 = load double, ptr %367, align 8
  %369 = fsub double %368, %349
  %370 = call double @llvm.fabs.f64(double %369)
  %371 = fcmp ogt double %370, %339
  %372 = select i1 %371, double %370, double %339
  %373 = call double @llvm.fabs.f64(double %368)
  %374 = fmul double %373, 0x3EB0C6F7A0B5ED8D
  %375 = fcmp ogt double %370, %374
  br i1 %375, label %376, label %379

376:                                              ; preds = %364
  %377 = fcmp ogt double %370, 0x3EB0C6F7A0B5ED8D
  %378 = select i1 %377, i32 0, i32 %340
  br label %380

379:                                              ; preds = %364
  br label %380

380:                                              ; preds = %376, %379
  %381 = phi i32 [ %340, %379 ], [ %378, %376 ]
  br label %382

382:                                              ; preds = %380
  %383 = add i64 %338, 1
  br label %337

384:                                              ; preds = %337
  %385 = add i64 %328, 1
  br label %327

386:                                              ; preds = %327
  %387 = icmp ne i32 %330, 0
  %388 = icmp ne i32 %266, 0
  br i1 %388, label %389, label %390

389:                                              ; preds = %386
  br label %391

390:                                              ; preds = %386
  br label %391

391:                                              ; preds = %389, %390
  %392 = phi ptr [ @str2, %390 ], [ @str1, %389 ]
  br label %393

393:                                              ; preds = %391
  %394 = call i32 (ptr, ...) @printf(ptr @str0, ptr %392, double %265)
  br i1 %387, label %395, label %396

395:                                              ; preds = %393
  br label %397

396:                                              ; preds = %393
  br label %397

397:                                              ; preds = %395, %396
  %398 = phi ptr [ @str2, %396 ], [ @str1, %395 ]
  br label %399

399:                                              ; preds = %397
  %400 = call i32 (ptr, ...) @printf(ptr @str3, ptr %398, double %329)
  %401 = call i32 (ptr, ...) @printf(ptr @str4, double %178, double %262)
  %402 = fcmp ogt double %262, 1.000000e-09
  %403 = fcmp ogt double %178, 1.000000e-09
  %404 = and i1 %402, %403
  br i1 %404, label %405, label %408

405:                                              ; preds = %399
  %406 = fdiv double %262, %178
  %407 = call i32 (ptr, ...) @printf(ptr @str5, double %406)
  br label %408

408:                                              ; preds = %405, %399
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
  br label %210

210:                                              ; preds = %213, %209
  %211 = phi i64 [ 0, %209 ], [ %216, %213 ]
  %212 = icmp slt i64 %211, 16384
  br i1 %212, label %213, label %217

213:                                              ; preds = %210
  %214 = getelementptr i64, ptr %12, i64 %211
  %215 = load i64, ptr %214, align 8
  call void @artsDbDecrementLatch(i64 %215)
  %216 = add i64 %211, 1
  br label %210

217:                                              ; preds = %210
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = getelementptr i64, ptr %1, i32 1
  %8 = load i64, ptr %7, align 8
  %9 = alloca i64, i64 1, align 8
  store i64 0, ptr %9, align 8
  %10 = alloca i64, i64 16384, align 8
  %11 = alloca ptr, i64 16384, align 8
  br label %12

12:                                               ; preds = %15, %4
  %13 = phi i64 [ 0, %4 ], [ %27, %15 ]
  %14 = icmp slt i64 %13, 16384
  br i1 %14, label %15, label %28

15:                                               ; preds = %12
  %16 = load i64, ptr %9, align 8
  %17 = mul i64 %16, 24
  %18 = trunc i64 %17 to i32
  %19 = getelementptr i8, ptr %3, i32 %18
  %20 = getelementptr { i64, i32, ptr }, ptr %19, i32 0, i32 0
  %21 = load i64, ptr %20, align 8
  %22 = getelementptr { i64, i32, ptr }, ptr %19, i32 0, i32 2
  %23 = load ptr, ptr %22, align 8
  %24 = getelementptr i64, ptr %10, i64 %13
  store i64 %21, ptr %24, align 8
  %25 = getelementptr ptr, ptr %11, i64 %13
  store ptr %23, ptr %25, align 8
  %26 = add i64 %16, 1
  store i64 %26, ptr %9, align 8
  %27 = add i64 %13, 1
  br label %12

28:                                               ; preds = %12
  br label %29

29:                                               ; preds = %111, %28
  %30 = phi i64 [ %6, %28 ], [ %112, %111 ]
  %31 = icmp slt i64 %30, %8
  br i1 %31, label %32, label %113

32:                                               ; preds = %29
  %33 = trunc i64 %30 to i32
  %34 = mul i32 %33, 128
  %35 = add i32 %34, %33
  %36 = sext i32 %35 to i64
  %37 = mul i64 %36, 8
  %38 = getelementptr i8, ptr %11, i64 %37
  br label %39

39:                                               ; preds = %42, %32
  %40 = phi i64 [ %6, %32 ], [ %54, %42 ]
  %41 = icmp slt i64 %40, %30
  br i1 %41, label %42, label %55

42:                                               ; preds = %39
  %43 = trunc i64 %40 to i32
  %44 = add i32 %34, %43
  %45 = sext i32 %44 to i64
  %46 = mul i64 %45, 8
  %47 = getelementptr i8, ptr %11, i64 %46
  %48 = load ptr, ptr %47, align 8
  %49 = load double, ptr %48, align 8
  %50 = fmul double %49, %49
  %51 = load ptr, ptr %38, align 8
  %52 = load double, ptr %51, align 8
  %53 = fsub double %52, %50
  store double %53, ptr %51, align 8
  %54 = add i64 %40, 1
  br label %39

55:                                               ; preds = %39
  %56 = load ptr, ptr %38, align 8
  %57 = load double, ptr %56, align 8
  %58 = fcmp olt double %57, 0.000000e+00
  br i1 %58, label %59, label %61

59:                                               ; preds = %55
  %60 = load ptr, ptr %38, align 8
  store double 0.000000e+00, ptr %60, align 8
  br label %61

61:                                               ; preds = %59, %55
  %62 = load ptr, ptr %38, align 8
  %63 = load double, ptr %62, align 8
  %64 = call double @llvm.sqrt.f64(double %63)
  store double %64, ptr %62, align 8
  %65 = add i32 %33, 1
  %66 = sext i32 %65 to i64
  br label %67

67:                                               ; preds = %109, %61
  %68 = phi i64 [ %66, %61 ], [ %110, %109 ]
  %69 = icmp slt i64 %68, %8
  br i1 %69, label %70, label %111

70:                                               ; preds = %67
  %71 = trunc i64 %68 to i32
  %72 = mul i32 %71, 128
  %73 = add i32 %72, %33
  %74 = sext i32 %73 to i64
  %75 = mul i64 %74, 8
  %76 = getelementptr i8, ptr %11, i64 %75
  br label %77

77:                                               ; preds = %80, %70
  %78 = phi i64 [ %6, %70 ], [ %98, %80 ]
  %79 = icmp slt i64 %78, %30
  br i1 %79, label %80, label %99

80:                                               ; preds = %77
  %81 = trunc i64 %78 to i32
  %82 = add i32 %72, %81
  %83 = sext i32 %82 to i64
  %84 = mul i64 %83, 8
  %85 = getelementptr i8, ptr %11, i64 %84
  %86 = load ptr, ptr %85, align 8
  %87 = load double, ptr %86, align 8
  %88 = add i32 %34, %81
  %89 = sext i32 %88 to i64
  %90 = mul i64 %89, 8
  %91 = getelementptr i8, ptr %11, i64 %90
  %92 = load ptr, ptr %91, align 8
  %93 = load double, ptr %92, align 8
  %94 = fmul double %87, %93
  %95 = load ptr, ptr %76, align 8
  %96 = load double, ptr %95, align 8
  %97 = fsub double %96, %94
  store double %97, ptr %95, align 8
  %98 = add i64 %78, 1
  br label %77

99:                                               ; preds = %77
  %100 = load ptr, ptr %38, align 8
  %101 = load double, ptr %100, align 8
  %102 = fcmp une double %101, 0.000000e+00
  br i1 %102, label %103, label %107

103:                                              ; preds = %99
  %104 = load ptr, ptr %76, align 8
  %105 = load double, ptr %104, align 8
  %106 = fdiv double %105, %101
  store double %106, ptr %104, align 8
  br label %109

107:                                              ; preds = %99
  %108 = load ptr, ptr %76, align 8
  store double 0.000000e+00, ptr %108, align 8
  br label %109

109:                                              ; preds = %103, %107
  %110 = add i64 %68, 1
  br label %67

111:                                              ; preds = %67
  %112 = add i64 %30, 1
  br label %29

113:                                              ; preds = %29
  br label %114

114:                                              ; preds = %117, %113
  %115 = phi i64 [ 0, %113 ], [ %120, %117 ]
  %116 = icmp slt i64 %115, 16384
  br i1 %116, label %117, label %121

117:                                              ; preds = %114
  %118 = getelementptr i64, ptr %10, i64 %115
  %119 = load i64, ptr %118, align 8
  call void @artsDbDecrementLatch(i64 %119)
  %120 = add i64 %115, 1
  br label %114

121:                                              ; preds = %114
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
  %14 = alloca i64, i64 16384, align 8
  %15 = alloca ptr, i64 16384, align 8
  br label %16

16:                                               ; preds = %19, %4
  %17 = phi i64 [ 0, %4 ], [ %31, %19 ]
  %18 = icmp slt i64 %17, 16384
  br i1 %18, label %19, label %32

19:                                               ; preds = %16
  %20 = load i64, ptr %13, align 8
  %21 = mul i64 %20, 24
  %22 = trunc i64 %21 to i32
  %23 = getelementptr i8, ptr %3, i32 %22
  %24 = getelementptr { i64, i32, ptr }, ptr %23, i32 0, i32 0
  %25 = load i64, ptr %24, align 8
  %26 = getelementptr { i64, i32, ptr }, ptr %23, i32 0, i32 2
  %27 = load ptr, ptr %26, align 8
  %28 = getelementptr i64, ptr %14, i64 %17
  store i64 %25, ptr %28, align 8
  %29 = getelementptr ptr, ptr %15, i64 %17
  store ptr %27, ptr %29, align 8
  %30 = add i64 %20, 1
  store i64 %30, ptr %13, align 8
  %31 = add i64 %17, 1
  br label %16

32:                                               ; preds = %16
  br label %33

33:                                               ; preds = %87, %32
  %34 = phi i64 [ %6, %32 ], [ %88, %87 ]
  %35 = icmp slt i64 %34, %12
  br i1 %35, label %36, label %89

36:                                               ; preds = %33
  %37 = trunc i64 %34 to i32
  %38 = mul i32 %37, 128
  %39 = add i32 %38, %37
  %40 = sext i32 %39 to i64
  %41 = mul i64 %40, 8
  %42 = getelementptr i8, ptr %15, i64 %41
  br label %43

43:                                               ; preds = %85, %36
  %44 = phi i64 [ %8, %36 ], [ %86, %85 ]
  %45 = icmp slt i64 %44, %10
  br i1 %45, label %46, label %87

46:                                               ; preds = %43
  %47 = trunc i64 %44 to i32
  %48 = mul i32 %47, 128
  %49 = add i32 %48, %37
  %50 = sext i32 %49 to i64
  %51 = mul i64 %50, 8
  %52 = getelementptr i8, ptr %15, i64 %51
  br label %53

53:                                               ; preds = %56, %46
  %54 = phi i64 [ %6, %46 ], [ %74, %56 ]
  %55 = icmp slt i64 %54, %34
  br i1 %55, label %56, label %75

56:                                               ; preds = %53
  %57 = trunc i64 %54 to i32
  %58 = add i32 %48, %57
  %59 = sext i32 %58 to i64
  %60 = mul i64 %59, 8
  %61 = getelementptr i8, ptr %15, i64 %60
  %62 = load ptr, ptr %61, align 8
  %63 = load double, ptr %62, align 8
  %64 = add i32 %38, %57
  %65 = sext i32 %64 to i64
  %66 = mul i64 %65, 8
  %67 = getelementptr i8, ptr %15, i64 %66
  %68 = load ptr, ptr %67, align 8
  %69 = load double, ptr %68, align 8
  %70 = fmul double %63, %69
  %71 = load ptr, ptr %52, align 8
  %72 = load double, ptr %71, align 8
  %73 = fsub double %72, %70
  store double %73, ptr %71, align 8
  %74 = add i64 %54, 1
  br label %53

75:                                               ; preds = %53
  %76 = load ptr, ptr %42, align 8
  %77 = load double, ptr %76, align 8
  %78 = fcmp une double %77, 0.000000e+00
  br i1 %78, label %79, label %83

79:                                               ; preds = %75
  %80 = load ptr, ptr %52, align 8
  %81 = load double, ptr %80, align 8
  %82 = fdiv double %81, %77
  store double %82, ptr %80, align 8
  br label %85

83:                                               ; preds = %75
  %84 = load ptr, ptr %52, align 8
  store double 0.000000e+00, ptr %84, align 8
  br label %85

85:                                               ; preds = %79, %83
  %86 = add i64 %44, 1
  br label %43

87:                                               ; preds = %43
  %88 = add i64 %34, 1
  br label %33

89:                                               ; preds = %33
  br label %90

90:                                               ; preds = %93, %89
  %91 = phi i64 [ 0, %89 ], [ %96, %93 ]
  %92 = icmp slt i64 %91, 16384
  br i1 %92, label %93, label %97

93:                                               ; preds = %90
  %94 = getelementptr i64, ptr %14, i64 %91
  %95 = load i64, ptr %94, align 8
  call void @artsDbDecrementLatch(i64 %95)
  %96 = add i64 %91, 1
  br label %90

97:                                               ; preds = %90
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
  %19 = alloca i64, i64 16384, align 8
  %20 = alloca ptr, i64 16384, align 8
  br label %21

21:                                               ; preds = %24, %4
  %22 = phi i64 [ 0, %4 ], [ %36, %24 ]
  %23 = icmp slt i64 %22, 16384
  br i1 %23, label %24, label %37

24:                                               ; preds = %21
  %25 = load i64, ptr %18, align 8
  %26 = mul i64 %25, 24
  %27 = trunc i64 %26 to i32
  %28 = getelementptr i8, ptr %3, i32 %27
  %29 = getelementptr { i64, i32, ptr }, ptr %28, i32 0, i32 0
  %30 = load i64, ptr %29, align 8
  %31 = getelementptr { i64, i32, ptr }, ptr %28, i32 0, i32 2
  %32 = load ptr, ptr %31, align 8
  %33 = getelementptr i64, ptr %19, i64 %22
  store i64 %30, ptr %33, align 8
  %34 = getelementptr ptr, ptr %20, i64 %22
  store ptr %32, ptr %34, align 8
  %35 = add i64 %25, 1
  store i64 %35, ptr %18, align 8
  %36 = add i64 %22, 1
  br label %21

37:                                               ; preds = %21
  br label %38

38:                                               ; preds = %85, %37
  %39 = phi i64 [ %15, %37 ], [ %86, %85 ]
  %40 = icmp slt i64 %39, %17
  br i1 %40, label %41, label %87

41:                                               ; preds = %38
  %42 = trunc i64 %39 to i32
  br label %43

43:                                               ; preds = %83, %41
  %44 = phi i64 [ %11, %41 ], [ %84, %83 ]
  %45 = icmp slt i64 %44, %13
  br i1 %45, label %46, label %85

46:                                               ; preds = %43
  %47 = trunc i64 %44 to i32
  %48 = icmp sge i32 %42, %47
  br i1 %48, label %49, label %81

49:                                               ; preds = %46
  %50 = mul i32 %42, 128
  %51 = mul i32 %47, 128
  br label %52

52:                                               ; preds = %56, %49
  %53 = phi i64 [ %7, %49 ], [ %72, %56 ]
  %54 = phi double [ 0.000000e+00, %49 ], [ %71, %56 ]
  %55 = icmp slt i64 %53, %9
  br i1 %55, label %56, label %73

56:                                               ; preds = %52
  %57 = trunc i64 %53 to i32
  %58 = add i32 %50, %57
  %59 = sext i32 %58 to i64
  %60 = mul i64 %59, 8
  %61 = getelementptr i8, ptr %20, i64 %60
  %62 = load ptr, ptr %61, align 8
  %63 = load double, ptr %62, align 8
  %64 = add i32 %51, %57
  %65 = sext i32 %64 to i64
  %66 = mul i64 %65, 8
  %67 = getelementptr i8, ptr %20, i64 %66
  %68 = load ptr, ptr %67, align 8
  %69 = load double, ptr %68, align 8
  %70 = fmul double %63, %69
  %71 = fadd double %54, %70
  %72 = add i64 %53, 1
  br label %52

73:                                               ; preds = %52
  %74 = add i32 %50, %47
  %75 = sext i32 %74 to i64
  %76 = mul i64 %75, 8
  %77 = getelementptr i8, ptr %20, i64 %76
  %78 = load ptr, ptr %77, align 8
  %79 = load double, ptr %78, align 8
  %80 = fsub double %79, %54
  store double %80, ptr %78, align 8
  br label %82

81:                                               ; preds = %46
  br label %82

82:                                               ; preds = %73, %81
  br label %83

83:                                               ; preds = %82
  %84 = add i64 %44, 1
  br label %43

85:                                               ; preds = %43
  %86 = add i64 %39, 1
  br label %38

87:                                               ; preds = %38
  br label %88

88:                                               ; preds = %91, %87
  %89 = phi i64 [ 0, %87 ], [ %94, %91 ]
  %90 = icmp slt i64 %89, 16384
  br i1 %90, label %91, label %95

91:                                               ; preds = %88
  %92 = getelementptr i64, ptr %19, i64 %89
  %93 = load i64, ptr %92, align 8
  call void @artsDbDecrementLatch(i64 %93)
  %94 = add i64 %89, 1
  br label %88

95:                                               ; preds = %88
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

; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@stdout = external global ptr
@str5 = internal constant [19 x i8] c"Result: INCORRECT\0A\00"
@str4 = internal constant [17 x i8] c"Result: CORRECT\0A\00"
@str3 = internal constant [53 x i8] c"Mismatch at (%d,%d): Parallel=%.2f, Sequential=%.2f\0A\00"
@str2 = internal constant [24 x i8] c"Finished in %f seconds\0A\00"
@str1 = internal constant [55 x i8] c"Matrix size and block size must be positive integers.\0A\00"
@str0 = internal constant [38 x i8] c"Usage: %s <matrix_size> <block_size>\0A\00"
@stderr = external global ptr

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

declare i32 @fprintf(ptr, ptr, ...)

define i32 @mainBody(i32 %0, ptr %1) {
  %3 = icmp ne i32 %0, 3
  %4 = icmp eq i32 %0, 3
  %5 = select i1 %3, i32 1, i32 undef
  br i1 %3, label %6, label %10

6:                                                ; preds = %2
  %7 = load ptr, ptr @stderr, align 8
  %8 = load ptr, ptr %1, align 8
  %9 = call i32 (ptr, ptr, ...) @fprintf(ptr %7, ptr @str0, ptr %8)
  br label %10

10:                                               ; preds = %6, %2
  br i1 %4, label %11, label %28

11:                                               ; preds = %10
  %12 = getelementptr ptr, ptr %1, i32 1
  %13 = load ptr, ptr %12, align 8
  %14 = call i32 @atoi(ptr %13)
  %15 = icmp sgt i32 %14, 0
  %16 = getelementptr ptr, ptr %1, i32 2
  %17 = load ptr, ptr %16, align 8
  %18 = call i32 @atoi(ptr %17)
  %19 = icmp sle i32 %18, 0
  %20 = icmp sle i32 %14, 0
  %21 = and i1 %15, %19
  %22 = or i1 %20, %21
  %23 = xor i1 %22, true
  %24 = select i1 %22, i32 1, i32 %5
  br i1 %22, label %25, label %28

25:                                               ; preds = %11
  %26 = load ptr, ptr @stderr, align 8
  %27 = call i32 (ptr, ptr, ...) @fprintf(ptr %26, ptr @str1)
  br label %28

28:                                               ; preds = %25, %11, %10
  %29 = phi i32 [ undef, %10 ], [ %14, %11 ], [ %14, %25 ]
  %30 = phi i32 [ undef, %10 ], [ %18, %11 ], [ %18, %25 ]
  %31 = phi i1 [ false, %10 ], [ %23, %11 ], [ %23, %25 ]
  %32 = phi i32 [ %5, %10 ], [ %24, %11 ], [ %24, %25 ]
  br label %33

33:                                               ; preds = %28
  %34 = phi i32 [ %29, %28 ]
  %35 = phi i32 [ %30, %28 ]
  %36 = phi i1 [ %31, %28 ]
  %37 = phi i32 [ %32, %28 ]
  br label %38

38:                                               ; preds = %33
  %39 = select i1 %36, i32 0, i32 %37
  br i1 %36, label %40, label %379

40:                                               ; preds = %38
  %41 = sext i32 %34 to i64
  %42 = call i32 @artsGetCurrentNode()
  %43 = mul i64 %41, 1
  %44 = mul i64 %43, %41
  %45 = mul i64 %44, 8
  %46 = alloca i8, i64 %45, align 1
  %47 = alloca i8, i64 %45, align 1
  br label %48

48:                                               ; preds = %65, %40
  %49 = phi i64 [ 0, %40 ], [ %66, %65 ]
  %50 = icmp slt i64 %49, %41
  br i1 %50, label %51, label %67

51:                                               ; preds = %48
  br label %52

52:                                               ; preds = %55, %51
  %53 = phi i64 [ 0, %51 ], [ %64, %55 ]
  %54 = icmp slt i64 %53, %41
  br i1 %54, label %55, label %65

55:                                               ; preds = %52
  %56 = call i64 @artsReserveGuidRoute(i32 10, i32 %42)
  %57 = call ptr @artsDbCreateWithGuid(i64 %56, i64 4)
  %58 = mul i64 %53, 1
  %59 = add i64 %58, 0
  %60 = mul i64 %49, %43
  %61 = add i64 %59, %60
  %62 = getelementptr i64, ptr %46, i64 %61
  store i64 %56, ptr %62, align 8
  %63 = getelementptr ptr, ptr %47, i64 %61
  store ptr %57, ptr %63, align 8
  %64 = add i64 %53, 1
  br label %52

65:                                               ; preds = %52
  %66 = add i64 %49, 1
  br label %48

67:                                               ; preds = %48
  %68 = call i32 @artsGetCurrentNode()
  %69 = alloca i8, i64 %45, align 1
  %70 = alloca i8, i64 %45, align 1
  br label %71

71:                                               ; preds = %88, %67
  %72 = phi i64 [ 0, %67 ], [ %89, %88 ]
  %73 = icmp slt i64 %72, %41
  br i1 %73, label %74, label %90

74:                                               ; preds = %71
  br label %75

75:                                               ; preds = %78, %74
  %76 = phi i64 [ 0, %74 ], [ %87, %78 ]
  %77 = icmp slt i64 %76, %41
  br i1 %77, label %78, label %88

78:                                               ; preds = %75
  %79 = call i64 @artsReserveGuidRoute(i32 10, i32 %68)
  %80 = call ptr @artsDbCreateWithGuid(i64 %79, i64 4)
  %81 = mul i64 %76, 1
  %82 = add i64 %81, 0
  %83 = mul i64 %72, %43
  %84 = add i64 %82, %83
  %85 = getelementptr i64, ptr %69, i64 %84
  store i64 %79, ptr %85, align 8
  %86 = getelementptr ptr, ptr %70, i64 %84
  store ptr %80, ptr %86, align 8
  %87 = add i64 %76, 1
  br label %75

88:                                               ; preds = %75
  %89 = add i64 %72, 1
  br label %71

90:                                               ; preds = %71
  %91 = call i32 @artsGetCurrentNode()
  %92 = alloca i8, i64 %45, align 1
  %93 = alloca i8, i64 %45, align 1
  br label %94

94:                                               ; preds = %111, %90
  %95 = phi i64 [ 0, %90 ], [ %112, %111 ]
  %96 = icmp slt i64 %95, %41
  br i1 %96, label %97, label %113

97:                                               ; preds = %94
  br label %98

98:                                               ; preds = %101, %97
  %99 = phi i64 [ 0, %97 ], [ %110, %101 ]
  %100 = icmp slt i64 %99, %41
  br i1 %100, label %101, label %111

101:                                              ; preds = %98
  %102 = call i64 @artsReserveGuidRoute(i32 10, i32 %91)
  %103 = call ptr @artsDbCreateWithGuid(i64 %102, i64 4)
  %104 = mul i64 %99, 1
  %105 = add i64 %104, 0
  %106 = mul i64 %95, %43
  %107 = add i64 %105, %106
  %108 = getelementptr i64, ptr %92, i64 %107
  store i64 %102, ptr %108, align 8
  %109 = getelementptr ptr, ptr %93, i64 %107
  store ptr %103, ptr %109, align 8
  %110 = add i64 %99, 1
  br label %98

111:                                              ; preds = %98
  %112 = add i64 %95, 1
  br label %94

113:                                              ; preds = %94
  %114 = mul i64 %44, 4
  %115 = alloca i8, i64 %114, align 1
  br label %116

116:                                              ; preds = %146, %113
  %117 = phi i64 [ 0, %113 ], [ %147, %146 ]
  %118 = icmp slt i64 %117, %41
  br i1 %118, label %119, label %148

119:                                              ; preds = %116
  %120 = trunc i64 %117 to i32
  %121 = mul i64 %117, %43
  %122 = getelementptr ptr, ptr %47, i64 %121
  %123 = getelementptr ptr, ptr %70, i64 %121
  %124 = getelementptr ptr, ptr %93, i64 %121
  br label %125

125:                                              ; preds = %128, %119
  %126 = phi i64 [ 0, %119 ], [ %145, %128 ]
  %127 = icmp slt i64 %126, %41
  br i1 %127, label %128, label %146

128:                                              ; preds = %125
  %129 = trunc i64 %126 to i32
  %130 = add i32 %120, %129
  %131 = sitofp i32 %130 to float
  %132 = mul i64 %126, 8
  %133 = getelementptr i8, ptr %122, i64 %132
  %134 = load ptr, ptr %133, align 8
  store float %131, ptr %134, align 4
  %135 = sub i32 %120, %129
  %136 = sitofp i32 %135 to float
  %137 = getelementptr i8, ptr %123, i64 %132
  %138 = load ptr, ptr %137, align 8
  store float %136, ptr %138, align 4
  %139 = getelementptr i8, ptr %124, i64 %132
  %140 = load ptr, ptr %139, align 8
  store float 0.000000e+00, ptr %140, align 4
  %141 = mul i64 %126, 1
  %142 = add i64 %141, 0
  %143 = add i64 %142, %121
  %144 = getelementptr float, ptr %115, i64 %143
  store float 0.000000e+00, ptr %144, align 4
  %145 = add i64 %126, 1
  br label %125

146:                                              ; preds = %125
  %147 = add i64 %117, 1
  br label %116

148:                                              ; preds = %116
  %149 = call double @omp_get_wtime()
  %150 = sext i32 %35 to i64
  %151 = call i32 @artsGetCurrentNode()
  %152 = alloca i64, i64 0, align 8
  %153 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %151, i32 0, ptr %152, i32 1)
  %154 = call i64 @artsInitializeAndStartEpoch(i64 %153, i32 0)
  %155 = call i32 @artsGetCurrentNode()
  %156 = mul i64 %41, %41
  %157 = add i64 %156, %156
  %158 = add i64 %157, %156
  %159 = trunc i64 %158 to i32
  %160 = alloca i64, i64 3, align 8
  store i64 %150, ptr %160, align 8
  %161 = getelementptr i64, ptr %160, i32 1
  store i64 %150, ptr %161, align 8
  %162 = getelementptr i64, ptr %160, i32 2
  store i64 %41, ptr %162, align 8
  %163 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %155, i32 3, ptr %160, i32 %159, i64 %154)
  %164 = call ptr @malloc(i64 8)
  store i64 0, ptr %164, align 8
  br label %165

165:                                              ; preds = %183, %148
  %166 = phi i64 [ 0, %148 ], [ %184, %183 ]
  %167 = icmp slt i64 %166, %41
  br i1 %167, label %168, label %185

168:                                              ; preds = %165
  br label %169

169:                                              ; preds = %172, %168
  %170 = phi i64 [ 0, %168 ], [ %182, %172 ]
  %171 = icmp slt i64 %170, %41
  br i1 %171, label %172, label %183

172:                                              ; preds = %169
  %173 = load i64, ptr %164, align 8
  %174 = mul i64 %170, 1
  %175 = add i64 %174, 0
  %176 = mul i64 %166, %43
  %177 = add i64 %175, %176
  %178 = getelementptr i64, ptr %69, i64 %177
  %179 = load i64, ptr %178, align 8
  %180 = trunc i64 %173 to i32
  call void @artsDbAddDependence(i64 %179, i64 %163, i32 %180)
  %181 = add i64 %173, 1
  store i64 %181, ptr %164, align 8
  %182 = add i64 %170, 1
  br label %169

183:                                              ; preds = %169
  %184 = add i64 %166, 1
  br label %165

185:                                              ; preds = %165
  %186 = call ptr @malloc(i64 8)
  store i64 %156, ptr %186, align 8
  br label %187

187:                                              ; preds = %205, %185
  %188 = phi i64 [ 0, %185 ], [ %206, %205 ]
  %189 = icmp slt i64 %188, %41
  br i1 %189, label %190, label %207

190:                                              ; preds = %187
  br label %191

191:                                              ; preds = %194, %190
  %192 = phi i64 [ 0, %190 ], [ %204, %194 ]
  %193 = icmp slt i64 %192, %41
  br i1 %193, label %194, label %205

194:                                              ; preds = %191
  %195 = load i64, ptr %186, align 8
  %196 = mul i64 %192, 1
  %197 = add i64 %196, 0
  %198 = mul i64 %188, %43
  %199 = add i64 %197, %198
  %200 = getelementptr i64, ptr %92, i64 %199
  %201 = load i64, ptr %200, align 8
  %202 = trunc i64 %195 to i32
  call void @artsDbAddDependence(i64 %201, i64 %163, i32 %202)
  %203 = add i64 %195, 1
  store i64 %203, ptr %186, align 8
  %204 = add i64 %192, 1
  br label %191

205:                                              ; preds = %191
  %206 = add i64 %188, 1
  br label %187

207:                                              ; preds = %187
  %208 = call ptr @malloc(i64 8)
  store i64 %157, ptr %208, align 8
  br label %209

209:                                              ; preds = %227, %207
  %210 = phi i64 [ 0, %207 ], [ %228, %227 ]
  %211 = icmp slt i64 %210, %41
  br i1 %211, label %212, label %229

212:                                              ; preds = %209
  br label %213

213:                                              ; preds = %216, %212
  %214 = phi i64 [ 0, %212 ], [ %226, %216 ]
  %215 = icmp slt i64 %214, %41
  br i1 %215, label %216, label %227

216:                                              ; preds = %213
  %217 = load i64, ptr %208, align 8
  %218 = mul i64 %214, 1
  %219 = add i64 %218, 0
  %220 = mul i64 %210, %43
  %221 = add i64 %219, %220
  %222 = getelementptr i64, ptr %46, i64 %221
  %223 = load i64, ptr %222, align 8
  %224 = trunc i64 %217 to i32
  call void @artsDbAddDependence(i64 %223, i64 %163, i32 %224)
  %225 = add i64 %217, 1
  store i64 %225, ptr %208, align 8
  %226 = add i64 %214, 1
  br label %213

227:                                              ; preds = %213
  %228 = add i64 %210, 1
  br label %209

229:                                              ; preds = %209
  %230 = call ptr @malloc(i64 8)
  store i64 0, ptr %230, align 8
  br label %231

231:                                              ; preds = %248, %229
  %232 = phi i64 [ 0, %229 ], [ %249, %248 ]
  %233 = icmp slt i64 %232, %41
  br i1 %233, label %234, label %250

234:                                              ; preds = %231
  br label %235

235:                                              ; preds = %238, %234
  %236 = phi i64 [ 0, %234 ], [ %247, %238 ]
  %237 = icmp slt i64 %236, %41
  br i1 %237, label %238, label %248

238:                                              ; preds = %235
  %239 = mul i64 %236, 1
  %240 = add i64 %239, 0
  %241 = mul i64 %232, %43
  %242 = add i64 %240, %241
  %243 = getelementptr i64, ptr %69, i64 %242
  %244 = load i64, ptr %243, align 8
  call void @artsDbIncrementLatch(i64 %244)
  %245 = load i64, ptr %230, align 8
  %246 = add i64 %245, 1
  store i64 %246, ptr %230, align 8
  %247 = add i64 %236, 1
  br label %235

248:                                              ; preds = %235
  %249 = add i64 %232, 1
  br label %231

250:                                              ; preds = %231
  %251 = call ptr @malloc(i64 8)
  store i64 %156, ptr %251, align 8
  br label %252

252:                                              ; preds = %269, %250
  %253 = phi i64 [ 0, %250 ], [ %270, %269 ]
  %254 = icmp slt i64 %253, %41
  br i1 %254, label %255, label %271

255:                                              ; preds = %252
  br label %256

256:                                              ; preds = %259, %255
  %257 = phi i64 [ 0, %255 ], [ %268, %259 ]
  %258 = icmp slt i64 %257, %41
  br i1 %258, label %259, label %269

259:                                              ; preds = %256
  %260 = mul i64 %257, 1
  %261 = add i64 %260, 0
  %262 = mul i64 %253, %43
  %263 = add i64 %261, %262
  %264 = getelementptr i64, ptr %92, i64 %263
  %265 = load i64, ptr %264, align 8
  call void @artsDbIncrementLatch(i64 %265)
  %266 = load i64, ptr %251, align 8
  %267 = add i64 %266, 1
  store i64 %267, ptr %251, align 8
  %268 = add i64 %257, 1
  br label %256

269:                                              ; preds = %256
  %270 = add i64 %253, 1
  br label %252

271:                                              ; preds = %252
  %272 = call ptr @malloc(i64 8)
  store i64 %157, ptr %272, align 8
  br label %273

273:                                              ; preds = %290, %271
  %274 = phi i64 [ 0, %271 ], [ %291, %290 ]
  %275 = icmp slt i64 %274, %41
  br i1 %275, label %276, label %292

276:                                              ; preds = %273
  br label %277

277:                                              ; preds = %280, %276
  %278 = phi i64 [ 0, %276 ], [ %289, %280 ]
  %279 = icmp slt i64 %278, %41
  br i1 %279, label %280, label %290

280:                                              ; preds = %277
  %281 = mul i64 %278, 1
  %282 = add i64 %281, 0
  %283 = mul i64 %274, %43
  %284 = add i64 %282, %283
  %285 = getelementptr i64, ptr %46, i64 %284
  %286 = load i64, ptr %285, align 8
  call void @artsDbIncrementLatch(i64 %286)
  %287 = load i64, ptr %272, align 8
  %288 = add i64 %287, 1
  store i64 %288, ptr %272, align 8
  %289 = add i64 %278, 1
  br label %277

290:                                              ; preds = %277
  %291 = add i64 %274, 1
  br label %273

292:                                              ; preds = %273
  %293 = call i1 @artsWaitOnHandle(i64 %154)
  %294 = call double @omp_get_wtime()
  %295 = fsub double %294, %149
  %296 = call i32 (ptr, ...) @printf(ptr @str2, double %295)
  br label %297

297:                                              ; preds = %331, %292
  %298 = phi i64 [ 0, %292 ], [ %332, %331 ]
  %299 = icmp slt i64 %298, %41
  br i1 %299, label %300, label %333

300:                                              ; preds = %297
  %301 = mul i64 %298, %43
  %302 = getelementptr ptr, ptr %47, i64 %301
  br label %303

303:                                              ; preds = %325, %300
  %304 = phi i64 [ 0, %300 ], [ %330, %325 ]
  %305 = icmp slt i64 %304, %41
  br i1 %305, label %306, label %331

306:                                              ; preds = %303
  %307 = mul i64 %304, 8
  br label %308

308:                                              ; preds = %312, %306
  %309 = phi i64 [ 0, %306 ], [ %324, %312 ]
  %310 = phi float [ 0.000000e+00, %306 ], [ %323, %312 ]
  %311 = icmp slt i64 %309, %41
  br i1 %311, label %312, label %325

312:                                              ; preds = %308
  %313 = mul i64 %309, 8
  %314 = getelementptr i8, ptr %302, i64 %313
  %315 = load ptr, ptr %314, align 8
  %316 = load float, ptr %315, align 4
  %317 = mul i64 %309, %43
  %318 = getelementptr ptr, ptr %70, i64 %317
  %319 = getelementptr i8, ptr %318, i64 %307
  %320 = load ptr, ptr %319, align 8
  %321 = load float, ptr %320, align 4
  %322 = fmul float %316, %321
  %323 = fadd float %310, %322
  %324 = add i64 %309, 1
  br label %308

325:                                              ; preds = %308
  %326 = mul i64 %304, 1
  %327 = add i64 %326, 0
  %328 = add i64 %327, %301
  %329 = getelementptr float, ptr %115, i64 %328
  store float %310, ptr %329, align 4
  %330 = add i64 %304, 1
  br label %303

331:                                              ; preds = %303
  %332 = add i64 %298, 1
  br label %297

333:                                              ; preds = %297
  br label %334

334:                                              ; preds = %368, %333
  %335 = phi i64 [ 0, %333 ], [ %369, %368 ]
  %336 = phi i32 [ 1, %333 ], [ %343, %368 ]
  %337 = icmp slt i64 %335, %41
  br i1 %337, label %338, label %370

338:                                              ; preds = %334
  %339 = mul i64 %335, %43
  %340 = getelementptr ptr, ptr %93, i64 %339
  br label %341

341:                                              ; preds = %366, %338
  %342 = phi i64 [ 0, %338 ], [ %367, %366 ]
  %343 = phi i32 [ %336, %338 ], [ %359, %366 ]
  %344 = icmp slt i64 %342, %41
  br i1 %344, label %345, label %368

345:                                              ; preds = %341
  %346 = mul i64 %342, 8
  %347 = getelementptr i8, ptr %340, i64 %346
  %348 = load ptr, ptr %347, align 8
  %349 = load float, ptr %348, align 4
  %350 = mul i64 %342, 1
  %351 = add i64 %350, 0
  %352 = add i64 %351, %339
  %353 = getelementptr float, ptr %115, i64 %352
  %354 = load float, ptr %353, align 4
  %355 = fsub float %349, %354
  %356 = fpext float %355 to double
  %357 = call double @llvm.fabs.f64(double %356)
  %358 = fcmp ogt double %357, 0x3EB0C6F7A0B5ED8D
  %359 = select i1 %358, i32 0, i32 %343
  br i1 %358, label %360, label %366

360:                                              ; preds = %345
  %361 = trunc i64 %342 to i32
  %362 = trunc i64 %335 to i32
  %363 = fpext float %349 to double
  %364 = fpext float %354 to double
  %365 = call i32 (ptr, ...) @printf(ptr @str3, i32 %362, i32 %361, double %363, double %364)
  br label %366

366:                                              ; preds = %360, %345
  %367 = add i64 %342, 1
  br label %341

368:                                              ; preds = %341
  %369 = add i64 %335, 1
  br label %334

370:                                              ; preds = %334
  %371 = icmp ne i32 %336, 0
  br i1 %371, label %372, label %374

372:                                              ; preds = %370
  %373 = call i32 (ptr, ...) @printf(ptr @str4)
  br label %376

374:                                              ; preds = %370
  %375 = call i32 (ptr, ...) @printf(ptr @str5)
  br label %376

376:                                              ; preds = %372, %374
  %377 = load ptr, ptr @stdout, align 8
  %378 = call i32 @fflush(ptr %377)
  br label %379

379:                                              ; preds = %376, %38
  ret i32 %39
}

declare i32 @atoi(ptr)

declare double @omp_get_wtime()

declare i32 @fflush(ptr)

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = getelementptr i64, ptr %1, i32 1
  %8 = load i64, ptr %7, align 8
  %9 = trunc i64 %8 to i32
  %10 = getelementptr i64, ptr %1, i32 2
  %11 = load i64, ptr %10, align 8
  %12 = alloca i64, i64 1, align 8
  store i64 0, ptr %12, align 8
  %13 = mul i64 %11, 1
  %14 = mul i64 %13, %11
  %15 = mul i64 %14, 8
  %16 = alloca i8, i64 %15, align 1
  br label %17

17:                                               ; preds = %38, %4
  %18 = phi i64 [ 0, %4 ], [ %39, %38 ]
  %19 = icmp slt i64 %18, %11
  br i1 %19, label %20, label %40

20:                                               ; preds = %17
  br label %21

21:                                               ; preds = %24, %20
  %22 = phi i64 [ 0, %20 ], [ %37, %24 ]
  %23 = icmp slt i64 %22, %11
  br i1 %23, label %24, label %38

24:                                               ; preds = %21
  %25 = load i64, ptr %12, align 8
  %26 = mul i64 %25, 24
  %27 = trunc i64 %26 to i32
  %28 = getelementptr i8, ptr %3, i32 %27
  %29 = getelementptr { i64, i32, ptr }, ptr %28, i32 0, i32 0
  %30 = load i64, ptr %29, align 8
  %31 = mul i64 %22, 1
  %32 = add i64 %31, 0
  %33 = mul i64 %18, %13
  %34 = add i64 %32, %33
  %35 = getelementptr i64, ptr %16, i64 %34
  store i64 %30, ptr %35, align 8
  %36 = add i64 %25, 1
  store i64 %36, ptr %12, align 8
  %37 = add i64 %22, 1
  br label %21

38:                                               ; preds = %21
  %39 = add i64 %18, 1
  br label %17

40:                                               ; preds = %17
  %41 = alloca i8, i64 %15, align 1
  br label %42

42:                                               ; preds = %63, %40
  %43 = phi i64 [ 0, %40 ], [ %64, %63 ]
  %44 = icmp slt i64 %43, %11
  br i1 %44, label %45, label %65

45:                                               ; preds = %42
  br label %46

46:                                               ; preds = %49, %45
  %47 = phi i64 [ 0, %45 ], [ %62, %49 ]
  %48 = icmp slt i64 %47, %11
  br i1 %48, label %49, label %63

49:                                               ; preds = %46
  %50 = load i64, ptr %12, align 8
  %51 = mul i64 %50, 24
  %52 = trunc i64 %51 to i32
  %53 = getelementptr i8, ptr %3, i32 %52
  %54 = getelementptr { i64, i32, ptr }, ptr %53, i32 0, i32 0
  %55 = load i64, ptr %54, align 8
  %56 = mul i64 %47, 1
  %57 = add i64 %56, 0
  %58 = mul i64 %43, %13
  %59 = add i64 %57, %58
  %60 = getelementptr i64, ptr %41, i64 %59
  store i64 %55, ptr %60, align 8
  %61 = add i64 %50, 1
  store i64 %61, ptr %12, align 8
  %62 = add i64 %47, 1
  br label %46

63:                                               ; preds = %46
  %64 = add i64 %43, 1
  br label %42

65:                                               ; preds = %42
  %66 = alloca i8, i64 %15, align 1
  br label %67

67:                                               ; preds = %88, %65
  %68 = phi i64 [ 0, %65 ], [ %89, %88 ]
  %69 = icmp slt i64 %68, %11
  br i1 %69, label %70, label %90

70:                                               ; preds = %67
  br label %71

71:                                               ; preds = %74, %70
  %72 = phi i64 [ 0, %70 ], [ %87, %74 ]
  %73 = icmp slt i64 %72, %11
  br i1 %73, label %74, label %88

74:                                               ; preds = %71
  %75 = load i64, ptr %12, align 8
  %76 = mul i64 %75, 24
  %77 = trunc i64 %76 to i32
  %78 = getelementptr i8, ptr %3, i32 %77
  %79 = getelementptr { i64, i32, ptr }, ptr %78, i32 0, i32 0
  %80 = load i64, ptr %79, align 8
  %81 = mul i64 %72, 1
  %82 = add i64 %81, 0
  %83 = mul i64 %68, %13
  %84 = add i64 %82, %83
  %85 = getelementptr i64, ptr %66, i64 %84
  store i64 %80, ptr %85, align 8
  %86 = add i64 %75, 1
  store i64 %86, ptr %12, align 8
  %87 = add i64 %72, 1
  br label %71

88:                                               ; preds = %71
  %89 = add i64 %68, 1
  br label %67

90:                                               ; preds = %67
  %91 = alloca i64, i64 1, align 8
  %92 = alloca i64, i64 1, align 8
  br label %93

93:                                               ; preds = %238, %90
  %94 = phi i64 [ 0, %90 ], [ %239, %238 ]
  %95 = icmp slt i64 %94, %11
  br i1 %95, label %96, label %240

96:                                               ; preds = %93
  %97 = udiv i64 %94, %6
  %98 = mul i64 %97, %6
  %99 = trunc i64 %98 to i32
  %100 = add i32 %99, %9
  %101 = sext i32 %100 to i64
  %102 = sub i64 %101, %98
  br label %103

103:                                              ; preds = %236, %96
  %104 = phi i64 [ 0, %96 ], [ %237, %236 ]
  %105 = icmp slt i64 %104, %11
  br i1 %105, label %106, label %238

106:                                              ; preds = %103
  %107 = udiv i64 %104, %6
  %108 = mul i64 %107, %6
  %109 = trunc i64 %108 to i32
  %110 = add i32 %109, %9
  %111 = sext i32 %110 to i64
  %112 = sub i64 %111, %108
  %113 = mul i64 %102, %112
  br label %114

114:                                              ; preds = %234, %106
  %115 = phi i64 [ 0, %106 ], [ %235, %234 ]
  %116 = icmp slt i64 %115, %11
  br i1 %116, label %117, label %236

117:                                              ; preds = %114
  %118 = udiv i64 %115, %6
  %119 = mul i64 %118, %6
  %120 = trunc i64 %119 to i32
  %121 = add i32 %120, %9
  %122 = sext i32 %121 to i64
  %123 = sub i64 %122, %119
  %124 = call i64 @artsGetCurrentEpochGuid()
  %125 = call i32 @artsGetCurrentNode()
  store i64 0, ptr %91, align 8
  %126 = load i64, ptr %91, align 8
  %127 = mul i64 %123, %112
  %128 = add i64 %126, %127
  store i64 %128, ptr %91, align 8
  %129 = load i64, ptr %91, align 8
  %130 = add i64 %129, %113
  store i64 %130, ptr %91, align 8
  %131 = load i64, ptr %91, align 8
  %132 = mul i64 %102, %123
  %133 = add i64 %131, %132
  store i64 %133, ptr %91, align 8
  %134 = load i64, ptr %91, align 8
  %135 = trunc i64 %134 to i32
  store i64 9, ptr %92, align 8
  %136 = load i64, ptr %92, align 8
  %137 = trunc i64 %136 to i32
  %138 = alloca i64, i64 %136, align 8
  store i64 %98, ptr %138, align 8
  %139 = getelementptr i64, ptr %138, i32 1
  store i64 %119, ptr %139, align 8
  %140 = getelementptr i64, ptr %138, i32 2
  store i64 %108, ptr %140, align 8
  %141 = getelementptr i64, ptr %138, i32 3
  store i64 %122, ptr %141, align 8
  %142 = getelementptr i64, ptr %138, i32 4
  store i64 %111, ptr %142, align 8
  %143 = getelementptr i64, ptr %138, i32 5
  store i64 %101, ptr %143, align 8
  %144 = getelementptr i64, ptr %138, i32 6
  store i64 %123, ptr %144, align 8
  %145 = getelementptr i64, ptr %138, i32 7
  store i64 %112, ptr %145, align 8
  %146 = getelementptr i64, ptr %138, i32 8
  store i64 %102, ptr %146, align 8
  %147 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %125, i32 %137, ptr %138, i32 %135, i64 %124)
  %148 = call ptr @malloc(i64 8)
  store i64 %126, ptr %148, align 8
  br label %149

149:                                              ; preds = %167, %117
  %150 = phi i64 [ %119, %117 ], [ %168, %167 ]
  %151 = icmp slt i64 %150, %122
  br i1 %151, label %152, label %169

152:                                              ; preds = %149
  br label %153

153:                                              ; preds = %156, %152
  %154 = phi i64 [ %108, %152 ], [ %166, %156 ]
  %155 = icmp slt i64 %154, %111
  br i1 %155, label %156, label %167

156:                                              ; preds = %153
  %157 = load i64, ptr %148, align 8
  %158 = mul i64 %154, 1
  %159 = add i64 %158, 0
  %160 = mul i64 %150, %13
  %161 = add i64 %159, %160
  %162 = getelementptr i64, ptr %16, i64 %161
  %163 = load i64, ptr %162, align 8
  %164 = trunc i64 %157 to i32
  call void @artsDbAddDependence(i64 %163, i64 %147, i32 %164)
  %165 = add i64 %157, 1
  store i64 %165, ptr %148, align 8
  %166 = add i64 %154, 1
  br label %153

167:                                              ; preds = %153
  %168 = add i64 %150, 1
  br label %149

169:                                              ; preds = %149
  %170 = call ptr @malloc(i64 8)
  store i64 %129, ptr %170, align 8
  br label %171

171:                                              ; preds = %189, %169
  %172 = phi i64 [ %98, %169 ], [ %190, %189 ]
  %173 = icmp slt i64 %172, %101
  br i1 %173, label %174, label %191

174:                                              ; preds = %171
  br label %175

175:                                              ; preds = %178, %174
  %176 = phi i64 [ %108, %174 ], [ %188, %178 ]
  %177 = icmp slt i64 %176, %111
  br i1 %177, label %178, label %189

178:                                              ; preds = %175
  %179 = load i64, ptr %170, align 8
  %180 = mul i64 %176, 1
  %181 = add i64 %180, 0
  %182 = mul i64 %172, %13
  %183 = add i64 %181, %182
  %184 = getelementptr i64, ptr %41, i64 %183
  %185 = load i64, ptr %184, align 8
  %186 = trunc i64 %179 to i32
  call void @artsDbAddDependence(i64 %185, i64 %147, i32 %186)
  %187 = add i64 %179, 1
  store i64 %187, ptr %170, align 8
  %188 = add i64 %176, 1
  br label %175

189:                                              ; preds = %175
  %190 = add i64 %172, 1
  br label %171

191:                                              ; preds = %171
  %192 = call ptr @malloc(i64 8)
  store i64 %131, ptr %192, align 8
  br label %193

193:                                              ; preds = %211, %191
  %194 = phi i64 [ %98, %191 ], [ %212, %211 ]
  %195 = icmp slt i64 %194, %101
  br i1 %195, label %196, label %213

196:                                              ; preds = %193
  br label %197

197:                                              ; preds = %200, %196
  %198 = phi i64 [ %119, %196 ], [ %210, %200 ]
  %199 = icmp slt i64 %198, %122
  br i1 %199, label %200, label %211

200:                                              ; preds = %197
  %201 = load i64, ptr %192, align 8
  %202 = mul i64 %198, 1
  %203 = add i64 %202, 0
  %204 = mul i64 %194, %13
  %205 = add i64 %203, %204
  %206 = getelementptr i64, ptr %66, i64 %205
  %207 = load i64, ptr %206, align 8
  %208 = trunc i64 %201 to i32
  call void @artsDbAddDependence(i64 %207, i64 %147, i32 %208)
  %209 = add i64 %201, 1
  store i64 %209, ptr %192, align 8
  %210 = add i64 %198, 1
  br label %197

211:                                              ; preds = %197
  %212 = add i64 %194, 1
  br label %193

213:                                              ; preds = %193
  %214 = call ptr @malloc(i64 8)
  store i64 %129, ptr %214, align 8
  br label %215

215:                                              ; preds = %232, %213
  %216 = phi i64 [ %98, %213 ], [ %233, %232 ]
  %217 = icmp slt i64 %216, %101
  br i1 %217, label %218, label %234

218:                                              ; preds = %215
  br label %219

219:                                              ; preds = %222, %218
  %220 = phi i64 [ %108, %218 ], [ %231, %222 ]
  %221 = icmp slt i64 %220, %111
  br i1 %221, label %222, label %232

222:                                              ; preds = %219
  %223 = mul i64 %220, 1
  %224 = add i64 %223, 0
  %225 = mul i64 %216, %13
  %226 = add i64 %224, %225
  %227 = getelementptr i64, ptr %41, i64 %226
  %228 = load i64, ptr %227, align 8
  call void @artsDbIncrementLatch(i64 %228)
  %229 = load i64, ptr %214, align 8
  %230 = add i64 %229, 1
  store i64 %230, ptr %214, align 8
  %231 = add i64 %220, 1
  br label %219

232:                                              ; preds = %219
  %233 = add i64 %216, 1
  br label %215

234:                                              ; preds = %215
  %235 = add i64 %115, %6
  br label %114

236:                                              ; preds = %114
  %237 = add i64 %104, %6
  br label %103

238:                                              ; preds = %103
  %239 = add i64 %94, %6
  br label %93

240:                                              ; preds = %93
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = getelementptr i64, ptr %1, i32 1
  %8 = load i64, ptr %7, align 8
  %9 = getelementptr i64, ptr %1, i32 2
  %10 = load i64, ptr %9, align 8
  %11 = getelementptr i64, ptr %1, i32 3
  %12 = load i64, ptr %11, align 8
  %13 = getelementptr i64, ptr %1, i32 4
  %14 = load i64, ptr %13, align 8
  %15 = getelementptr i64, ptr %1, i32 5
  %16 = load i64, ptr %15, align 8
  %17 = getelementptr i64, ptr %1, i32 6
  %18 = load i64, ptr %17, align 8
  %19 = getelementptr i64, ptr %1, i32 7
  %20 = load i64, ptr %19, align 8
  %21 = getelementptr i64, ptr %1, i32 8
  %22 = load i64, ptr %21, align 8
  %23 = alloca i64, i64 1, align 8
  store i64 0, ptr %23, align 8
  %24 = mul i64 %18, 1
  %25 = mul i64 %24, %20
  %26 = mul i64 %25, 8
  %27 = alloca i8, i64 %26, align 1
  br label %28

28:                                               ; preds = %50, %4
  %29 = phi i64 [ 0, %4 ], [ %51, %50 ]
  %30 = icmp slt i64 %29, %18
  br i1 %30, label %31, label %52

31:                                               ; preds = %28
  br label %32

32:                                               ; preds = %35, %31
  %33 = phi i64 [ 0, %31 ], [ %49, %35 ]
  %34 = icmp slt i64 %33, %20
  br i1 %34, label %35, label %50

35:                                               ; preds = %32
  %36 = load i64, ptr %23, align 8
  %37 = mul i64 %36, 24
  %38 = trunc i64 %37 to i32
  %39 = getelementptr i8, ptr %3, i32 %38
  %40 = getelementptr { i64, i32, ptr }, ptr %39, i32 0, i32 2
  %41 = load ptr, ptr %40, align 8
  %42 = mul i64 %33, 1
  %43 = add i64 %42, 0
  %44 = mul i64 %20, 1
  %45 = mul i64 %29, %44
  %46 = add i64 %43, %45
  %47 = getelementptr ptr, ptr %27, i64 %46
  store ptr %41, ptr %47, align 8
  %48 = add i64 %36, 1
  store i64 %48, ptr %23, align 8
  %49 = add i64 %33, 1
  br label %32

50:                                               ; preds = %32
  %51 = add i64 %29, 1
  br label %28

52:                                               ; preds = %28
  %53 = mul i64 %22, 1
  %54 = mul i64 %53, %20
  %55 = mul i64 %54, 8
  %56 = alloca i8, i64 %55, align 1
  br label %57

57:                                               ; preds = %79, %52
  %58 = phi i64 [ 0, %52 ], [ %80, %79 ]
  %59 = icmp slt i64 %58, %22
  br i1 %59, label %60, label %81

60:                                               ; preds = %57
  br label %61

61:                                               ; preds = %64, %60
  %62 = phi i64 [ 0, %60 ], [ %78, %64 ]
  %63 = icmp slt i64 %62, %20
  br i1 %63, label %64, label %79

64:                                               ; preds = %61
  %65 = load i64, ptr %23, align 8
  %66 = mul i64 %65, 24
  %67 = trunc i64 %66 to i32
  %68 = getelementptr i8, ptr %3, i32 %67
  %69 = getelementptr { i64, i32, ptr }, ptr %68, i32 0, i32 2
  %70 = load ptr, ptr %69, align 8
  %71 = mul i64 %62, 1
  %72 = add i64 %71, 0
  %73 = mul i64 %20, 1
  %74 = mul i64 %58, %73
  %75 = add i64 %72, %74
  %76 = getelementptr ptr, ptr %56, i64 %75
  store ptr %70, ptr %76, align 8
  %77 = add i64 %65, 1
  store i64 %77, ptr %23, align 8
  %78 = add i64 %62, 1
  br label %61

79:                                               ; preds = %61
  %80 = add i64 %58, 1
  br label %57

81:                                               ; preds = %57
  %82 = mul i64 %53, %18
  %83 = mul i64 %82, 8
  %84 = alloca i8, i64 %83, align 1
  br label %85

85:                                               ; preds = %106, %81
  %86 = phi i64 [ 0, %81 ], [ %107, %106 ]
  %87 = icmp slt i64 %86, %22
  br i1 %87, label %88, label %108

88:                                               ; preds = %85
  br label %89

89:                                               ; preds = %92, %88
  %90 = phi i64 [ 0, %88 ], [ %105, %92 ]
  %91 = icmp slt i64 %90, %18
  br i1 %91, label %92, label %106

92:                                               ; preds = %89
  %93 = load i64, ptr %23, align 8
  %94 = mul i64 %93, 24
  %95 = trunc i64 %94 to i32
  %96 = getelementptr i8, ptr %3, i32 %95
  %97 = getelementptr { i64, i32, ptr }, ptr %96, i32 0, i32 2
  %98 = load ptr, ptr %97, align 8
  %99 = mul i64 %90, 1
  %100 = add i64 %99, 0
  %101 = mul i64 %86, %24
  %102 = add i64 %100, %101
  %103 = getelementptr ptr, ptr %84, i64 %102
  store ptr %98, ptr %103, align 8
  %104 = add i64 %93, 1
  store i64 %104, ptr %23, align 8
  %105 = add i64 %90, 1
  br label %89

106:                                              ; preds = %89
  %107 = add i64 %86, 1
  br label %85

108:                                              ; preds = %85
  br label %109

109:                                              ; preds = %147, %108
  %110 = phi i64 [ %6, %108 ], [ %148, %147 ]
  %111 = icmp slt i64 %110, %16
  br i1 %111, label %112, label %149

112:                                              ; preds = %109
  %113 = sub i64 %110, %6
  %114 = mul i64 %113, %24
  %115 = getelementptr ptr, ptr %84, i64 %114
  %116 = mul i64 %20, 1
  %117 = mul i64 %113, %116
  %118 = getelementptr ptr, ptr %56, i64 %117
  br label %119

119:                                              ; preds = %145, %112
  %120 = phi i64 [ %10, %112 ], [ %146, %145 ]
  %121 = icmp slt i64 %120, %14
  br i1 %121, label %122, label %147

122:                                              ; preds = %119
  %123 = sub i64 %120, %10
  %124 = mul i64 %123, 8
  %125 = getelementptr i8, ptr %118, i64 %124
  br label %126

126:                                              ; preds = %129, %122
  %127 = phi i64 [ %8, %122 ], [ %144, %129 ]
  %128 = icmp slt i64 %127, %12
  br i1 %128, label %129, label %145

129:                                              ; preds = %126
  %130 = sub i64 %127, %8
  %131 = mul i64 %130, 8
  %132 = getelementptr i8, ptr %115, i64 %131
  %133 = load ptr, ptr %132, align 8
  %134 = load float, ptr %133, align 4
  %135 = mul i64 %130, %116
  %136 = getelementptr ptr, ptr %27, i64 %135
  %137 = getelementptr i8, ptr %136, i64 %124
  %138 = load ptr, ptr %137, align 8
  %139 = load float, ptr %138, align 4
  %140 = fmul float %134, %139
  %141 = load ptr, ptr %125, align 8
  %142 = load float, ptr %141, align 4
  %143 = fadd float %142, %140
  store float %143, ptr %141, align 4
  %144 = add i64 %127, 1
  br label %126

145:                                              ; preds = %126
  %146 = add i64 %120, 1
  br label %119

147:                                              ; preds = %119
  %148 = add i64 %110, 1
  br label %109

149:                                              ; preds = %109
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

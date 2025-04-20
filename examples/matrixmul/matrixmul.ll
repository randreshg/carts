; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str12 = internal constant [20 x i8] c"Verification FAILED\00"
@str11 = internal constant [20 x i8] c"Verification PASSED\00"
@str10 = internal constant [4 x i8] c"%s\0A\00"
@str9 = internal constant [24 x i8] c"----------------------\0A\00"
@str8 = internal constant [25 x i8] c"\0AMatrix C (Sequential):\0A\00"
@str7 = internal constant [23 x i8] c"\0AMatrix C (Parallel):\0A\00"
@str6 = internal constant [12 x i8] c"\0AMatrix B:\0A\00"
@str5 = internal constant [2 x i8] c"\0A\00"
@str4 = internal constant [7 x i8] c"%6.2f \00"
@str3 = internal constant [11 x i8] c"Matrix A:\0A\00"
@str2 = internal constant [53 x i8] c"Mismatch at (%d,%d): Parallel=%.2f, Sequential=%.2f\0A\00"
@str1 = internal constant [55 x i8] c"Matrix size and block size must be positive integers.\0A\00"
@str0 = internal constant [38 x i8] c"Usage: %s <matrix_size> <block_size>\0A\00"
@stderr = external global ptr

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @artsRT(i32, ptr)

declare void @artsShutdown()

declare void @artsPersistentEventIncrementLatch(i64, i64)

declare void @artsPersistentEventDecrementLatch(i64, i64)

declare i64 @artsGetCurrentEpochGuid()

declare void @artsAddDependenceToPersistentEvent(i64, i64, i32, i64)

declare i64 @artsEdtCreateWithEpoch(ptr, i32, i32, ptr, i32, i64)

declare i64 @artsPersistentEventCreate(i32, i32, i64)

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
  %39 = sext i32 %34 to i64
  %40 = select i1 %36, i32 0, i32 %37
  br i1 %36, label %41, label %476

41:                                               ; preds = %38
  %42 = call i32 @artsGetCurrentNode()
  %43 = mul i64 %39, 1
  %44 = mul i64 %43, %39
  %45 = mul i64 %44, 8
  %46 = alloca i8, i64 %45, align 1
  br label %47

47:                                               ; preds = %62, %41
  %48 = phi i64 [ 0, %41 ], [ %63, %62 ]
  %49 = icmp slt i64 %48, %39
  br i1 %49, label %50, label %64

50:                                               ; preds = %47
  br label %51

51:                                               ; preds = %54, %50
  %52 = phi i64 [ 0, %50 ], [ %61, %54 ]
  %53 = icmp slt i64 %52, %39
  br i1 %53, label %54, label %62

54:                                               ; preds = %51
  %55 = call i64 @artsPersistentEventCreate(i32 %42, i32 0, i64 0)
  %56 = mul i64 %52, 1
  %57 = add i64 %56, 0
  %58 = mul i64 %48, %43
  %59 = add i64 %57, %58
  %60 = getelementptr i64, ptr %46, i64 %59
  store i64 %55, ptr %60, align 8
  %61 = add i64 %52, 1
  br label %51

62:                                               ; preds = %51
  %63 = add i64 %48, 1
  br label %47

64:                                               ; preds = %47
  %65 = call i32 @artsGetCurrentNode()
  %66 = alloca i8, i64 %45, align 1
  %67 = alloca i8, i64 %45, align 1
  br label %68

68:                                               ; preds = %85, %64
  %69 = phi i64 [ 0, %64 ], [ %86, %85 ]
  %70 = icmp slt i64 %69, %39
  br i1 %70, label %71, label %87

71:                                               ; preds = %68
  br label %72

72:                                               ; preds = %75, %71
  %73 = phi i64 [ 0, %71 ], [ %84, %75 ]
  %74 = icmp slt i64 %73, %39
  br i1 %74, label %75, label %85

75:                                               ; preds = %72
  %76 = call i64 @artsReserveGuidRoute(i32 8, i32 %65)
  %77 = call ptr @artsDbCreateWithGuid(i64 %76, i64 4)
  %78 = mul i64 %73, 1
  %79 = add i64 %78, 0
  %80 = mul i64 %69, %43
  %81 = add i64 %79, %80
  %82 = getelementptr i64, ptr %66, i64 %81
  store i64 %76, ptr %82, align 8
  %83 = getelementptr ptr, ptr %67, i64 %81
  store ptr %77, ptr %83, align 8
  %84 = add i64 %73, 1
  br label %72

85:                                               ; preds = %72
  %86 = add i64 %69, 1
  br label %68

87:                                               ; preds = %68
  %88 = call i32 @artsGetCurrentNode()
  %89 = alloca i8, i64 %45, align 1
  br label %90

90:                                               ; preds = %105, %87
  %91 = phi i64 [ 0, %87 ], [ %106, %105 ]
  %92 = icmp slt i64 %91, %39
  br i1 %92, label %93, label %107

93:                                               ; preds = %90
  br label %94

94:                                               ; preds = %97, %93
  %95 = phi i64 [ 0, %93 ], [ %104, %97 ]
  %96 = icmp slt i64 %95, %39
  br i1 %96, label %97, label %105

97:                                               ; preds = %94
  %98 = call i64 @artsPersistentEventCreate(i32 %88, i32 0, i64 0)
  %99 = mul i64 %95, 1
  %100 = add i64 %99, 0
  %101 = mul i64 %91, %43
  %102 = add i64 %100, %101
  %103 = getelementptr i64, ptr %89, i64 %102
  store i64 %98, ptr %103, align 8
  %104 = add i64 %95, 1
  br label %94

105:                                              ; preds = %94
  %106 = add i64 %91, 1
  br label %90

107:                                              ; preds = %90
  %108 = call i32 @artsGetCurrentNode()
  %109 = alloca i8, i64 %45, align 1
  %110 = alloca i8, i64 %45, align 1
  br label %111

111:                                              ; preds = %128, %107
  %112 = phi i64 [ 0, %107 ], [ %129, %128 ]
  %113 = icmp slt i64 %112, %39
  br i1 %113, label %114, label %130

114:                                              ; preds = %111
  br label %115

115:                                              ; preds = %118, %114
  %116 = phi i64 [ 0, %114 ], [ %127, %118 ]
  %117 = icmp slt i64 %116, %39
  br i1 %117, label %118, label %128

118:                                              ; preds = %115
  %119 = call i64 @artsReserveGuidRoute(i32 8, i32 %108)
  %120 = call ptr @artsDbCreateWithGuid(i64 %119, i64 4)
  %121 = mul i64 %116, 1
  %122 = add i64 %121, 0
  %123 = mul i64 %112, %43
  %124 = add i64 %122, %123
  %125 = getelementptr i64, ptr %109, i64 %124
  store i64 %119, ptr %125, align 8
  %126 = getelementptr ptr, ptr %110, i64 %124
  store ptr %120, ptr %126, align 8
  %127 = add i64 %116, 1
  br label %115

128:                                              ; preds = %115
  %129 = add i64 %112, 1
  br label %111

130:                                              ; preds = %111
  %131 = call i32 @artsGetCurrentNode()
  %132 = alloca i8, i64 %45, align 1
  br label %133

133:                                              ; preds = %148, %130
  %134 = phi i64 [ 0, %130 ], [ %149, %148 ]
  %135 = icmp slt i64 %134, %39
  br i1 %135, label %136, label %150

136:                                              ; preds = %133
  br label %137

137:                                              ; preds = %140, %136
  %138 = phi i64 [ 0, %136 ], [ %147, %140 ]
  %139 = icmp slt i64 %138, %39
  br i1 %139, label %140, label %148

140:                                              ; preds = %137
  %141 = call i64 @artsPersistentEventCreate(i32 %131, i32 0, i64 0)
  %142 = mul i64 %138, 1
  %143 = add i64 %142, 0
  %144 = mul i64 %134, %43
  %145 = add i64 %143, %144
  %146 = getelementptr i64, ptr %132, i64 %145
  store i64 %141, ptr %146, align 8
  %147 = add i64 %138, 1
  br label %137

148:                                              ; preds = %137
  %149 = add i64 %134, 1
  br label %133

150:                                              ; preds = %133
  %151 = call i32 @artsGetCurrentNode()
  %152 = alloca i8, i64 %45, align 1
  %153 = alloca i8, i64 %45, align 1
  br label %154

154:                                              ; preds = %171, %150
  %155 = phi i64 [ 0, %150 ], [ %172, %171 ]
  %156 = icmp slt i64 %155, %39
  br i1 %156, label %157, label %173

157:                                              ; preds = %154
  br label %158

158:                                              ; preds = %161, %157
  %159 = phi i64 [ 0, %157 ], [ %170, %161 ]
  %160 = icmp slt i64 %159, %39
  br i1 %160, label %161, label %171

161:                                              ; preds = %158
  %162 = call i64 @artsReserveGuidRoute(i32 10, i32 %151)
  %163 = call ptr @artsDbCreateWithGuid(i64 %162, i64 4)
  %164 = mul i64 %159, 1
  %165 = add i64 %164, 0
  %166 = mul i64 %155, %43
  %167 = add i64 %165, %166
  %168 = getelementptr i64, ptr %152, i64 %167
  store i64 %162, ptr %168, align 8
  %169 = getelementptr ptr, ptr %153, i64 %167
  store ptr %163, ptr %169, align 8
  %170 = add i64 %159, 1
  br label %158

171:                                              ; preds = %158
  %172 = add i64 %155, 1
  br label %154

173:                                              ; preds = %154
  %174 = mul i64 %44, 4
  %175 = alloca i8, i64 %174, align 1
  br label %176

176:                                              ; preds = %206, %173
  %177 = phi i64 [ 0, %173 ], [ %207, %206 ]
  %178 = icmp slt i64 %177, %39
  br i1 %178, label %179, label %208

179:                                              ; preds = %176
  %180 = trunc i64 %177 to i32
  %181 = mul i64 %177, %43
  %182 = getelementptr ptr, ptr %67, i64 %181
  %183 = getelementptr ptr, ptr %110, i64 %181
  %184 = getelementptr ptr, ptr %153, i64 %181
  br label %185

185:                                              ; preds = %188, %179
  %186 = phi i64 [ 0, %179 ], [ %205, %188 ]
  %187 = icmp slt i64 %186, %39
  br i1 %187, label %188, label %206

188:                                              ; preds = %185
  %189 = trunc i64 %186 to i32
  %190 = add i32 %180, %189
  %191 = sitofp i32 %190 to float
  %192 = mul i64 %186, 8
  %193 = getelementptr i8, ptr %182, i64 %192
  %194 = load ptr, ptr %193, align 8
  store float %191, ptr %194, align 4
  %195 = sub i32 %180, %189
  %196 = sitofp i32 %195 to float
  %197 = getelementptr i8, ptr %183, i64 %192
  %198 = load ptr, ptr %197, align 8
  store float %196, ptr %198, align 4
  %199 = getelementptr i8, ptr %184, i64 %192
  %200 = load ptr, ptr %199, align 8
  store float 0.000000e+00, ptr %200, align 4
  %201 = mul i64 %186, 1
  %202 = add i64 %201, 0
  %203 = add i64 %202, %181
  %204 = getelementptr float, ptr %175, i64 %203
  store float 0.000000e+00, ptr %204, align 4
  %205 = add i64 %186, 1
  br label %185

206:                                              ; preds = %185
  %207 = add i64 %177, 1
  br label %176

208:                                              ; preds = %176
  %209 = sext i32 %35 to i64
  %210 = call i32 @artsGetCurrentNode()
  %211 = alloca i64, i64 0, align 8
  %212 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %210, i32 0, ptr %211, i32 1)
  %213 = call i64 @artsInitializeAndStartEpoch(i64 %212, i32 0)
  %214 = call i32 @artsGetCurrentNode()
  %215 = mul i64 %39, %39
  %216 = add i64 %215, %215
  %217 = add i64 %216, %215
  %218 = trunc i64 %217 to i32
  %219 = alloca i64, i64 11, align 8
  store i64 %39, ptr %219, align 8
  %220 = getelementptr i64, ptr %219, i32 1
  store i64 %209, ptr %220, align 8
  %221 = getelementptr i64, ptr %219, i32 2
  store i64 %209, ptr %221, align 8
  %222 = getelementptr i64, ptr %219, i32 3
  store i64 %39, ptr %222, align 8
  %223 = getelementptr i64, ptr %219, i32 4
  store i64 %39, ptr %223, align 8
  %224 = getelementptr i64, ptr %219, i32 5
  store i64 %39, ptr %224, align 8
  %225 = getelementptr i64, ptr %219, i32 6
  store i64 %39, ptr %225, align 8
  %226 = getelementptr i64, ptr %219, i32 7
  store i64 %39, ptr %226, align 8
  %227 = getelementptr i64, ptr %219, i32 8
  store i64 %39, ptr %227, align 8
  %228 = getelementptr i64, ptr %219, i32 9
  store i64 %39, ptr %228, align 8
  %229 = getelementptr i64, ptr %219, i32 10
  store i64 %39, ptr %229, align 8
  %230 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %214, i32 11, ptr %219, i32 %218, i64 %213)
  %231 = call ptr @malloc(i64 8)
  store i64 0, ptr %231, align 8
  br label %232

232:                                              ; preds = %252, %208
  %233 = phi i64 [ 0, %208 ], [ %253, %252 ]
  %234 = icmp slt i64 %233, %39
  br i1 %234, label %235, label %254

235:                                              ; preds = %232
  br label %236

236:                                              ; preds = %239, %235
  %237 = phi i64 [ 0, %235 ], [ %251, %239 ]
  %238 = icmp slt i64 %237, %39
  br i1 %238, label %239, label %252

239:                                              ; preds = %236
  %240 = mul i64 %237, 1
  %241 = add i64 %240, 0
  %242 = mul i64 %233, %43
  %243 = add i64 %241, %242
  %244 = getelementptr i64, ptr %132, i64 %243
  %245 = load i64, ptr %244, align 8
  %246 = load i64, ptr %231, align 8
  %247 = getelementptr i64, ptr %152, i64 %243
  %248 = load i64, ptr %247, align 8
  %249 = trunc i64 %246 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %245, i64 %230, i32 %249, i64 %248)
  %250 = add i64 %246, 1
  store i64 %250, ptr %231, align 8
  %251 = add i64 %237, 1
  br label %236

252:                                              ; preds = %236
  %253 = add i64 %233, 1
  br label %232

254:                                              ; preds = %232
  %255 = call ptr @malloc(i64 8)
  store i64 %215, ptr %255, align 8
  br label %256

256:                                              ; preds = %276, %254
  %257 = phi i64 [ 0, %254 ], [ %277, %276 ]
  %258 = icmp slt i64 %257, %39
  br i1 %258, label %259, label %278

259:                                              ; preds = %256
  br label %260

260:                                              ; preds = %263, %259
  %261 = phi i64 [ 0, %259 ], [ %275, %263 ]
  %262 = icmp slt i64 %261, %39
  br i1 %262, label %263, label %276

263:                                              ; preds = %260
  %264 = mul i64 %261, 1
  %265 = add i64 %264, 0
  %266 = mul i64 %257, %43
  %267 = add i64 %265, %266
  %268 = getelementptr i64, ptr %89, i64 %267
  %269 = load i64, ptr %268, align 8
  %270 = load i64, ptr %255, align 8
  %271 = getelementptr i64, ptr %109, i64 %267
  %272 = load i64, ptr %271, align 8
  %273 = trunc i64 %270 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %269, i64 %230, i32 %273, i64 %272)
  %274 = add i64 %270, 1
  store i64 %274, ptr %255, align 8
  %275 = add i64 %261, 1
  br label %260

276:                                              ; preds = %260
  %277 = add i64 %257, 1
  br label %256

278:                                              ; preds = %256
  %279 = call ptr @malloc(i64 8)
  store i64 %216, ptr %279, align 8
  br label %280

280:                                              ; preds = %300, %278
  %281 = phi i64 [ 0, %278 ], [ %301, %300 ]
  %282 = icmp slt i64 %281, %39
  br i1 %282, label %283, label %302

283:                                              ; preds = %280
  br label %284

284:                                              ; preds = %287, %283
  %285 = phi i64 [ 0, %283 ], [ %299, %287 ]
  %286 = icmp slt i64 %285, %39
  br i1 %286, label %287, label %300

287:                                              ; preds = %284
  %288 = mul i64 %285, 1
  %289 = add i64 %288, 0
  %290 = mul i64 %281, %43
  %291 = add i64 %289, %290
  %292 = getelementptr i64, ptr %46, i64 %291
  %293 = load i64, ptr %292, align 8
  %294 = load i64, ptr %279, align 8
  %295 = getelementptr i64, ptr %66, i64 %291
  %296 = load i64, ptr %295, align 8
  %297 = trunc i64 %294 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %293, i64 %230, i32 %297, i64 %296)
  %298 = add i64 %294, 1
  store i64 %298, ptr %279, align 8
  %299 = add i64 %285, 1
  br label %284

300:                                              ; preds = %284
  %301 = add i64 %281, 1
  br label %280

302:                                              ; preds = %280
  %303 = call i1 @artsWaitOnHandle(i64 %213)
  br label %304

304:                                              ; preds = %338, %302
  %305 = phi i64 [ 0, %302 ], [ %339, %338 ]
  %306 = icmp slt i64 %305, %39
  br i1 %306, label %307, label %340

307:                                              ; preds = %304
  %308 = mul i64 %305, %43
  %309 = getelementptr ptr, ptr %67, i64 %308
  br label %310

310:                                              ; preds = %332, %307
  %311 = phi i64 [ 0, %307 ], [ %337, %332 ]
  %312 = icmp slt i64 %311, %39
  br i1 %312, label %313, label %338

313:                                              ; preds = %310
  %314 = mul i64 %311, 8
  br label %315

315:                                              ; preds = %319, %313
  %316 = phi i64 [ 0, %313 ], [ %331, %319 ]
  %317 = phi float [ 0.000000e+00, %313 ], [ %330, %319 ]
  %318 = icmp slt i64 %316, %39
  br i1 %318, label %319, label %332

319:                                              ; preds = %315
  %320 = mul i64 %316, 8
  %321 = getelementptr i8, ptr %309, i64 %320
  %322 = load ptr, ptr %321, align 8
  %323 = load float, ptr %322, align 4
  %324 = mul i64 %316, %43
  %325 = getelementptr ptr, ptr %110, i64 %324
  %326 = getelementptr i8, ptr %325, i64 %314
  %327 = load ptr, ptr %326, align 8
  %328 = load float, ptr %327, align 4
  %329 = fmul float %323, %328
  %330 = fadd float %317, %329
  %331 = add i64 %316, 1
  br label %315

332:                                              ; preds = %315
  %333 = mul i64 %311, 1
  %334 = add i64 %333, 0
  %335 = add i64 %334, %308
  %336 = getelementptr float, ptr %175, i64 %335
  store float %317, ptr %336, align 4
  %337 = add i64 %311, 1
  br label %310

338:                                              ; preds = %310
  %339 = add i64 %305, 1
  br label %304

340:                                              ; preds = %304
  br label %341

341:                                              ; preds = %375, %340
  %342 = phi i64 [ 0, %340 ], [ %376, %375 ]
  %343 = phi i32 [ 1, %340 ], [ %351, %375 ]
  %344 = icmp slt i64 %342, %39
  br i1 %344, label %345, label %377

345:                                              ; preds = %341
  %346 = trunc i64 %342 to i32
  %347 = mul i64 %342, %43
  %348 = getelementptr ptr, ptr %153, i64 %347
  br label %349

349:                                              ; preds = %373, %345
  %350 = phi i64 [ 0, %345 ], [ %374, %373 ]
  %351 = phi i32 [ %343, %345 ], [ %368, %373 ]
  %352 = icmp slt i64 %350, %39
  br i1 %352, label %353, label %375

353:                                              ; preds = %349
  %354 = trunc i64 %350 to i32
  %355 = mul i64 %350, 8
  %356 = getelementptr i8, ptr %348, i64 %355
  %357 = load ptr, ptr %356, align 8
  %358 = load float, ptr %357, align 4
  %359 = mul i64 %350, 1
  %360 = add i64 %359, 0
  %361 = add i64 %360, %347
  %362 = getelementptr float, ptr %175, i64 %361
  %363 = load float, ptr %362, align 4
  %364 = fsub float %358, %363
  %365 = fpext float %364 to double
  %366 = call double @llvm.fabs.f64(double %365)
  %367 = fcmp ogt double %366, 0x3EB0C6F7A0B5ED8D
  %368 = select i1 %367, i32 0, i32 %351
  br i1 %367, label %369, label %373

369:                                              ; preds = %353
  %370 = fpext float %358 to double
  %371 = fpext float %363 to double
  %372 = call i32 (ptr, ...) @printf(ptr @str2, i32 %346, i32 %354, double %370, double %371)
  br label %373

373:                                              ; preds = %369, %353
  %374 = add i64 %350, 1
  br label %349

375:                                              ; preds = %349
  %376 = add i64 %342, 1
  br label %341

377:                                              ; preds = %341
  %378 = icmp ne i32 %343, 0
  %379 = call i32 (ptr, ...) @printf(ptr @str3)
  br label %380

380:                                              ; preds = %397, %377
  %381 = phi i64 [ 0, %377 ], [ %399, %397 ]
  %382 = icmp slt i64 %381, %39
  br i1 %382, label %383, label %400

383:                                              ; preds = %380
  %384 = mul i64 %381, %43
  %385 = getelementptr ptr, ptr %67, i64 %384
  br label %386

386:                                              ; preds = %389, %383
  %387 = phi i64 [ 0, %383 ], [ %396, %389 ]
  %388 = icmp slt i64 %387, %39
  br i1 %388, label %389, label %397

389:                                              ; preds = %386
  %390 = mul i64 %387, 8
  %391 = getelementptr i8, ptr %385, i64 %390
  %392 = load ptr, ptr %391, align 8
  %393 = load float, ptr %392, align 4
  %394 = fpext float %393 to double
  %395 = call i32 (ptr, ...) @printf(ptr @str4, double %394)
  %396 = add i64 %387, 1
  br label %386

397:                                              ; preds = %386
  %398 = call i32 (ptr, ...) @printf(ptr @str5)
  %399 = add i64 %381, 1
  br label %380

400:                                              ; preds = %380
  %401 = call i32 (ptr, ...) @printf(ptr @str6)
  br label %402

402:                                              ; preds = %419, %400
  %403 = phi i64 [ 0, %400 ], [ %421, %419 ]
  %404 = icmp slt i64 %403, %39
  br i1 %404, label %405, label %422

405:                                              ; preds = %402
  %406 = mul i64 %403, %43
  %407 = getelementptr ptr, ptr %110, i64 %406
  br label %408

408:                                              ; preds = %411, %405
  %409 = phi i64 [ 0, %405 ], [ %418, %411 ]
  %410 = icmp slt i64 %409, %39
  br i1 %410, label %411, label %419

411:                                              ; preds = %408
  %412 = mul i64 %409, 8
  %413 = getelementptr i8, ptr %407, i64 %412
  %414 = load ptr, ptr %413, align 8
  %415 = load float, ptr %414, align 4
  %416 = fpext float %415 to double
  %417 = call i32 (ptr, ...) @printf(ptr @str4, double %416)
  %418 = add i64 %409, 1
  br label %408

419:                                              ; preds = %408
  %420 = call i32 (ptr, ...) @printf(ptr @str5)
  %421 = add i64 %403, 1
  br label %402

422:                                              ; preds = %402
  %423 = call i32 (ptr, ...) @printf(ptr @str7)
  br label %424

424:                                              ; preds = %441, %422
  %425 = phi i64 [ 0, %422 ], [ %443, %441 ]
  %426 = icmp slt i64 %425, %39
  br i1 %426, label %427, label %444

427:                                              ; preds = %424
  %428 = mul i64 %425, %43
  %429 = getelementptr ptr, ptr %153, i64 %428
  br label %430

430:                                              ; preds = %433, %427
  %431 = phi i64 [ 0, %427 ], [ %440, %433 ]
  %432 = icmp slt i64 %431, %39
  br i1 %432, label %433, label %441

433:                                              ; preds = %430
  %434 = mul i64 %431, 8
  %435 = getelementptr i8, ptr %429, i64 %434
  %436 = load ptr, ptr %435, align 8
  %437 = load float, ptr %436, align 4
  %438 = fpext float %437 to double
  %439 = call i32 (ptr, ...) @printf(ptr @str4, double %438)
  %440 = add i64 %431, 1
  br label %430

441:                                              ; preds = %430
  %442 = call i32 (ptr, ...) @printf(ptr @str5)
  %443 = add i64 %425, 1
  br label %424

444:                                              ; preds = %424
  %445 = call i32 (ptr, ...) @printf(ptr @str8)
  br label %446

446:                                              ; preds = %463, %444
  %447 = phi i64 [ 0, %444 ], [ %465, %463 ]
  %448 = icmp slt i64 %447, %39
  br i1 %448, label %449, label %466

449:                                              ; preds = %446
  br label %450

450:                                              ; preds = %453, %449
  %451 = phi i64 [ 0, %449 ], [ %462, %453 ]
  %452 = icmp slt i64 %451, %39
  br i1 %452, label %453, label %463

453:                                              ; preds = %450
  %454 = mul i64 %451, 1
  %455 = add i64 %454, 0
  %456 = mul i64 %447, %43
  %457 = add i64 %455, %456
  %458 = getelementptr float, ptr %175, i64 %457
  %459 = load float, ptr %458, align 4
  %460 = fpext float %459 to double
  %461 = call i32 (ptr, ...) @printf(ptr @str4, double %460)
  %462 = add i64 %451, 1
  br label %450

463:                                              ; preds = %450
  %464 = call i32 (ptr, ...) @printf(ptr @str5)
  %465 = add i64 %447, 1
  br label %446

466:                                              ; preds = %446
  %467 = call i32 (ptr, ...) @printf(ptr @str5)
  %468 = call i32 (ptr, ...) @printf(ptr @str9)
  br i1 %378, label %469, label %470

469:                                              ; preds = %466
  br label %471

470:                                              ; preds = %466
  br label %471

471:                                              ; preds = %469, %470
  %472 = phi ptr [ @str12, %470 ], [ @str11, %469 ]
  br label %473

473:                                              ; preds = %471
  %474 = call i32 (ptr, ...) @printf(ptr @str10, ptr %472)
  %475 = call i32 (ptr, ...) @printf(ptr @str9)
  br label %476

476:                                              ; preds = %473, %38
  ret i32 %40
}

declare i32 @atoi(ptr)

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = getelementptr i64, ptr %1, i32 1
  %6 = load i64, ptr %5, align 8
  %7 = getelementptr i64, ptr %1, i32 2
  %8 = load i64, ptr %7, align 8
  %9 = trunc i64 %8 to i32
  %10 = getelementptr i64, ptr %1, i32 3
  %11 = load i64, ptr %10, align 8
  %12 = trunc i64 %11 to i32
  %13 = getelementptr i64, ptr %1, i32 4
  %14 = load i64, ptr %13, align 8
  %15 = getelementptr i64, ptr %1, i32 10
  %16 = load i64, ptr %15, align 8
  %17 = alloca i64, i64 1, align 8
  store i64 0, ptr %17, align 8
  %18 = mul i64 %16, 1
  %19 = mul i64 %18, %16
  %20 = mul i64 %19, 8
  %21 = alloca i8, i64 %20, align 1
  br label %22

22:                                               ; preds = %43, %4
  %23 = phi i64 [ 0, %4 ], [ %44, %43 ]
  %24 = icmp slt i64 %23, %16
  br i1 %24, label %25, label %45

25:                                               ; preds = %22
  br label %26

26:                                               ; preds = %29, %25
  %27 = phi i64 [ 0, %25 ], [ %42, %29 ]
  %28 = icmp slt i64 %27, %16
  br i1 %28, label %29, label %43

29:                                               ; preds = %26
  %30 = load i64, ptr %17, align 8
  %31 = mul i64 %30, 24
  %32 = trunc i64 %31 to i32
  %33 = getelementptr i8, ptr %3, i32 %32
  %34 = getelementptr { i64, i32, ptr }, ptr %33, i32 0, i32 0
  %35 = load i64, ptr %34, align 8
  %36 = mul i64 %27, 1
  %37 = add i64 %36, 0
  %38 = mul i64 %23, %18
  %39 = add i64 %37, %38
  %40 = getelementptr i64, ptr %21, i64 %39
  store i64 %35, ptr %40, align 8
  %41 = add i64 %30, 1
  store i64 %41, ptr %17, align 8
  %42 = add i64 %27, 1
  br label %26

43:                                               ; preds = %26
  %44 = add i64 %23, 1
  br label %22

45:                                               ; preds = %22
  %46 = alloca i8, i64 %20, align 1
  br label %47

47:                                               ; preds = %68, %45
  %48 = phi i64 [ 0, %45 ], [ %69, %68 ]
  %49 = icmp slt i64 %48, %16
  br i1 %49, label %50, label %70

50:                                               ; preds = %47
  br label %51

51:                                               ; preds = %54, %50
  %52 = phi i64 [ 0, %50 ], [ %67, %54 ]
  %53 = icmp slt i64 %52, %16
  br i1 %53, label %54, label %68

54:                                               ; preds = %51
  %55 = load i64, ptr %17, align 8
  %56 = mul i64 %55, 24
  %57 = trunc i64 %56 to i32
  %58 = getelementptr i8, ptr %3, i32 %57
  %59 = getelementptr { i64, i32, ptr }, ptr %58, i32 0, i32 0
  %60 = load i64, ptr %59, align 8
  %61 = mul i64 %52, 1
  %62 = add i64 %61, 0
  %63 = mul i64 %48, %18
  %64 = add i64 %62, %63
  %65 = getelementptr i64, ptr %46, i64 %64
  store i64 %60, ptr %65, align 8
  %66 = add i64 %55, 1
  store i64 %66, ptr %17, align 8
  %67 = add i64 %52, 1
  br label %51

68:                                               ; preds = %51
  %69 = add i64 %48, 1
  br label %47

70:                                               ; preds = %47
  %71 = alloca i8, i64 %20, align 1
  br label %72

72:                                               ; preds = %93, %70
  %73 = phi i64 [ 0, %70 ], [ %94, %93 ]
  %74 = icmp slt i64 %73, %16
  br i1 %74, label %75, label %95

75:                                               ; preds = %72
  br label %76

76:                                               ; preds = %79, %75
  %77 = phi i64 [ 0, %75 ], [ %92, %79 ]
  %78 = icmp slt i64 %77, %16
  br i1 %78, label %79, label %93

79:                                               ; preds = %76
  %80 = load i64, ptr %17, align 8
  %81 = mul i64 %80, 24
  %82 = trunc i64 %81 to i32
  %83 = getelementptr i8, ptr %3, i32 %82
  %84 = getelementptr { i64, i32, ptr }, ptr %83, i32 0, i32 0
  %85 = load i64, ptr %84, align 8
  %86 = mul i64 %77, 1
  %87 = add i64 %86, 0
  %88 = mul i64 %73, %18
  %89 = add i64 %87, %88
  %90 = getelementptr i64, ptr %71, i64 %89
  store i64 %85, ptr %90, align 8
  %91 = add i64 %80, 1
  store i64 %91, ptr %17, align 8
  %92 = add i64 %77, 1
  br label %76

93:                                               ; preds = %76
  %94 = add i64 %73, 1
  br label %72

95:                                               ; preds = %72
  %96 = call i32 @artsGetCurrentNode()
  %97 = alloca i8, i64 %20, align 1
  br label %98

98:                                               ; preds = %113, %95
  %99 = phi i64 [ 0, %95 ], [ %114, %113 ]
  %100 = icmp slt i64 %99, %16
  br i1 %100, label %101, label %115

101:                                              ; preds = %98
  br label %102

102:                                              ; preds = %105, %101
  %103 = phi i64 [ 0, %101 ], [ %112, %105 ]
  %104 = icmp slt i64 %103, %16
  br i1 %104, label %105, label %113

105:                                              ; preds = %102
  %106 = call i64 @artsPersistentEventCreate(i32 %96, i32 0, i64 0)
  %107 = mul i64 %103, 1
  %108 = add i64 %107, 0
  %109 = mul i64 %99, %18
  %110 = add i64 %108, %109
  %111 = getelementptr i64, ptr %97, i64 %110
  store i64 %106, ptr %111, align 8
  %112 = add i64 %103, 1
  br label %102

113:                                              ; preds = %102
  %114 = add i64 %99, 1
  br label %98

115:                                              ; preds = %98
  %116 = call i32 @artsGetCurrentNode()
  %117 = alloca i8, i64 %20, align 1
  br label %118

118:                                              ; preds = %133, %115
  %119 = phi i64 [ 0, %115 ], [ %134, %133 ]
  %120 = icmp slt i64 %119, %16
  br i1 %120, label %121, label %135

121:                                              ; preds = %118
  br label %122

122:                                              ; preds = %125, %121
  %123 = phi i64 [ 0, %121 ], [ %132, %125 ]
  %124 = icmp slt i64 %123, %16
  br i1 %124, label %125, label %133

125:                                              ; preds = %122
  %126 = call i64 @artsPersistentEventCreate(i32 %116, i32 0, i64 0)
  %127 = mul i64 %123, 1
  %128 = add i64 %127, 0
  %129 = mul i64 %119, %18
  %130 = add i64 %128, %129
  %131 = getelementptr i64, ptr %117, i64 %130
  store i64 %126, ptr %131, align 8
  %132 = add i64 %123, 1
  br label %122

133:                                              ; preds = %122
  %134 = add i64 %119, 1
  br label %118

135:                                              ; preds = %118
  %136 = call i32 @artsGetCurrentNode()
  %137 = alloca i8, i64 %20, align 1
  br label %138

138:                                              ; preds = %153, %135
  %139 = phi i64 [ 0, %135 ], [ %154, %153 ]
  %140 = icmp slt i64 %139, %16
  br i1 %140, label %141, label %155

141:                                              ; preds = %138
  br label %142

142:                                              ; preds = %145, %141
  %143 = phi i64 [ 0, %141 ], [ %152, %145 ]
  %144 = icmp slt i64 %143, %16
  br i1 %144, label %145, label %153

145:                                              ; preds = %142
  %146 = call i64 @artsPersistentEventCreate(i32 %136, i32 0, i64 0)
  %147 = mul i64 %143, 1
  %148 = add i64 %147, 0
  %149 = mul i64 %139, %18
  %150 = add i64 %148, %149
  %151 = getelementptr i64, ptr %137, i64 %150
  store i64 %146, ptr %151, align 8
  %152 = add i64 %143, 1
  br label %142

153:                                              ; preds = %142
  %154 = add i64 %139, 1
  br label %138

155:                                              ; preds = %138
  %156 = alloca i64, i64 1, align 8
  %157 = sub i64 %6, 1
  %158 = add i64 %157, %14
  %159 = udiv i64 %158, %6
  %160 = sext i32 %9 to i64
  %161 = sext i32 %12 to i64
  %162 = mul i64 %16, %16
  %163 = add i64 %162, %162
  %164 = add i64 %163, %162
  %165 = trunc i64 %164 to i32
  %166 = add i64 %162, 13
  %167 = trunc i64 %166 to i32
  %168 = alloca i64, i64 %166, align 8
  br label %169

169:                                              ; preds = %329, %155
  %170 = phi i64 [ 0, %155 ], [ %330, %329 ]
  %171 = icmp slt i64 %170, %159
  br i1 %171, label %172, label %331

172:                                              ; preds = %169
  %173 = mul i64 %170, %6
  %174 = udiv i64 %173, %6
  %175 = mul i64 %174, %160
  %176 = trunc i64 %175 to i32
  %177 = add i32 %176, %9
  %178 = sext i32 %177 to i64
  br label %179

179:                                              ; preds = %327, %172
  %180 = phi i64 [ 0, %172 ], [ %328, %327 ]
  %181 = icmp slt i64 %180, %161
  br i1 %181, label %182, label %329

182:                                              ; preds = %179
  %183 = udiv i64 %180, %160
  %184 = mul i64 %183, %160
  %185 = trunc i64 %184 to i32
  %186 = sext i32 %185 to i64
  br label %187

187:                                              ; preds = %325, %182
  %188 = phi i64 [ 0, %182 ], [ %326, %325 ]
  %189 = icmp slt i64 %188, %161
  br i1 %189, label %190, label %327

190:                                              ; preds = %187
  %191 = udiv i64 %188, %160
  %192 = mul i64 %191, %160
  %193 = trunc i64 %192 to i32
  %194 = call i64 @artsGetCurrentEpochGuid()
  %195 = call i32 @artsGetCurrentNode()
  store i64 %186, ptr %168, align 8
  %196 = getelementptr i64, ptr %168, i32 1
  store i64 %160, ptr %196, align 8
  %197 = sext i32 %193 to i64
  %198 = getelementptr i64, ptr %168, i32 2
  store i64 %197, ptr %198, align 8
  %199 = getelementptr i64, ptr %168, i32 3
  store i64 %192, ptr %199, align 8
  %200 = getelementptr i64, ptr %168, i32 4
  store i64 %184, ptr %200, align 8
  %201 = getelementptr i64, ptr %168, i32 5
  store i64 %175, ptr %201, align 8
  %202 = getelementptr i64, ptr %168, i32 6
  store i64 %178, ptr %202, align 8
  %203 = getelementptr i64, ptr %168, i32 7
  store i64 %16, ptr %203, align 8
  %204 = getelementptr i64, ptr %168, i32 8
  store i64 %16, ptr %204, align 8
  %205 = getelementptr i64, ptr %168, i32 9
  store i64 %16, ptr %205, align 8
  %206 = getelementptr i64, ptr %168, i32 10
  store i64 %16, ptr %206, align 8
  %207 = getelementptr i64, ptr %168, i32 11
  store i64 %16, ptr %207, align 8
  %208 = getelementptr i64, ptr %168, i32 12
  store i64 %16, ptr %208, align 8
  store i64 13, ptr %156, align 8
  br label %209

209:                                              ; preds = %227, %190
  %210 = phi i64 [ 0, %190 ], [ %228, %227 ]
  %211 = icmp slt i64 %210, %16
  br i1 %211, label %212, label %229

212:                                              ; preds = %209
  br label %213

213:                                              ; preds = %216, %212
  %214 = phi i64 [ 0, %212 ], [ %226, %216 ]
  %215 = icmp slt i64 %214, %16
  br i1 %215, label %216, label %227

216:                                              ; preds = %213
  %217 = mul i64 %214, 1
  %218 = add i64 %217, 0
  %219 = mul i64 %210, %18
  %220 = add i64 %218, %219
  %221 = getelementptr i64, ptr %137, i64 %220
  %222 = load i64, ptr %221, align 8
  %223 = load i64, ptr %156, align 8
  %224 = getelementptr i64, ptr %168, i64 %223
  store i64 %222, ptr %224, align 8
  %225 = add i64 %223, 1
  store i64 %225, ptr %156, align 8
  %226 = add i64 %214, 1
  br label %213

227:                                              ; preds = %213
  %228 = add i64 %210, 1
  br label %209

229:                                              ; preds = %209
  %230 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %195, i32 %167, ptr %168, i32 %165, i64 %194)
  %231 = call ptr @malloc(i64 8)
  store i64 0, ptr %231, align 8
  br label %232

232:                                              ; preds = %252, %229
  %233 = phi i64 [ 0, %229 ], [ %253, %252 ]
  %234 = icmp slt i64 %233, %16
  br i1 %234, label %235, label %254

235:                                              ; preds = %232
  br label %236

236:                                              ; preds = %239, %235
  %237 = phi i64 [ 0, %235 ], [ %251, %239 ]
  %238 = icmp slt i64 %237, %16
  br i1 %238, label %239, label %252

239:                                              ; preds = %236
  %240 = mul i64 %237, 1
  %241 = add i64 %240, 0
  %242 = mul i64 %233, %18
  %243 = add i64 %241, %242
  %244 = getelementptr i64, ptr %137, i64 %243
  %245 = load i64, ptr %244, align 8
  %246 = load i64, ptr %231, align 8
  %247 = getelementptr i64, ptr %21, i64 %243
  %248 = load i64, ptr %247, align 8
  %249 = trunc i64 %246 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %245, i64 %230, i32 %249, i64 %248)
  %250 = add i64 %246, 1
  store i64 %250, ptr %231, align 8
  %251 = add i64 %237, 1
  br label %236

252:                                              ; preds = %236
  %253 = add i64 %233, 1
  br label %232

254:                                              ; preds = %232
  %255 = call ptr @malloc(i64 8)
  store i64 %162, ptr %255, align 8
  br label %256

256:                                              ; preds = %276, %254
  %257 = phi i64 [ 0, %254 ], [ %277, %276 ]
  %258 = icmp slt i64 %257, %16
  br i1 %258, label %259, label %278

259:                                              ; preds = %256
  br label %260

260:                                              ; preds = %263, %259
  %261 = phi i64 [ 0, %259 ], [ %275, %263 ]
  %262 = icmp slt i64 %261, %16
  br i1 %262, label %263, label %276

263:                                              ; preds = %260
  %264 = mul i64 %261, 1
  %265 = add i64 %264, 0
  %266 = mul i64 %257, %18
  %267 = add i64 %265, %266
  %268 = getelementptr i64, ptr %117, i64 %267
  %269 = load i64, ptr %268, align 8
  %270 = load i64, ptr %255, align 8
  %271 = getelementptr i64, ptr %46, i64 %267
  %272 = load i64, ptr %271, align 8
  %273 = trunc i64 %270 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %269, i64 %230, i32 %273, i64 %272)
  %274 = add i64 %270, 1
  store i64 %274, ptr %255, align 8
  %275 = add i64 %261, 1
  br label %260

276:                                              ; preds = %260
  %277 = add i64 %257, 1
  br label %256

278:                                              ; preds = %256
  %279 = call ptr @malloc(i64 8)
  store i64 %163, ptr %279, align 8
  br label %280

280:                                              ; preds = %300, %278
  %281 = phi i64 [ 0, %278 ], [ %301, %300 ]
  %282 = icmp slt i64 %281, %16
  br i1 %282, label %283, label %302

283:                                              ; preds = %280
  br label %284

284:                                              ; preds = %287, %283
  %285 = phi i64 [ 0, %283 ], [ %299, %287 ]
  %286 = icmp slt i64 %285, %16
  br i1 %286, label %287, label %300

287:                                              ; preds = %284
  %288 = mul i64 %285, 1
  %289 = add i64 %288, 0
  %290 = mul i64 %281, %18
  %291 = add i64 %289, %290
  %292 = getelementptr i64, ptr %97, i64 %291
  %293 = load i64, ptr %292, align 8
  %294 = load i64, ptr %279, align 8
  %295 = getelementptr i64, ptr %71, i64 %291
  %296 = load i64, ptr %295, align 8
  %297 = trunc i64 %294 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %293, i64 %230, i32 %297, i64 %296)
  %298 = add i64 %294, 1
  store i64 %298, ptr %279, align 8
  %299 = add i64 %285, 1
  br label %284

300:                                              ; preds = %284
  %301 = add i64 %281, 1
  br label %280

302:                                              ; preds = %280
  %303 = call ptr @malloc(i64 8)
  store i64 0, ptr %303, align 8
  br label %304

304:                                              ; preds = %323, %302
  %305 = phi i64 [ 0, %302 ], [ %324, %323 ]
  %306 = icmp slt i64 %305, %16
  br i1 %306, label %307, label %325

307:                                              ; preds = %304
  br label %308

308:                                              ; preds = %311, %307
  %309 = phi i64 [ 0, %307 ], [ %322, %311 ]
  %310 = icmp slt i64 %309, %16
  br i1 %310, label %311, label %323

311:                                              ; preds = %308
  %312 = mul i64 %309, 1
  %313 = add i64 %312, 0
  %314 = mul i64 %305, %18
  %315 = add i64 %313, %314
  %316 = getelementptr i64, ptr %137, i64 %315
  %317 = load i64, ptr %316, align 8
  %318 = getelementptr i64, ptr %21, i64 %315
  %319 = load i64, ptr %318, align 8
  call void @artsPersistentEventIncrementLatch(i64 %317, i64 %319)
  %320 = load i64, ptr %303, align 8
  %321 = add i64 %320, 1
  store i64 %321, ptr %303, align 8
  %322 = add i64 %309, 1
  br label %308

323:                                              ; preds = %308
  %324 = add i64 %305, 1
  br label %304

325:                                              ; preds = %304
  %326 = add i64 %188, %160
  br label %187

327:                                              ; preds = %187
  %328 = add i64 %180, %160
  br label %179

329:                                              ; preds = %179
  %330 = add i64 %170, 1
  br label %169

331:                                              ; preds = %169
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = sext i32 %6 to i64
  %8 = getelementptr i64, ptr %1, i32 1
  %9 = load i64, ptr %8, align 8
  %10 = trunc i64 %9 to i32
  %11 = sext i32 %10 to i64
  %12 = getelementptr i64, ptr %1, i32 2
  %13 = load i64, ptr %12, align 8
  %14 = trunc i64 %13 to i32
  %15 = sext i32 %14 to i64
  %16 = getelementptr i64, ptr %1, i32 3
  %17 = load i64, ptr %16, align 8
  %18 = getelementptr i64, ptr %1, i32 4
  %19 = load i64, ptr %18, align 8
  %20 = getelementptr i64, ptr %1, i32 5
  %21 = load i64, ptr %20, align 8
  %22 = getelementptr i64, ptr %1, i32 6
  %23 = load i64, ptr %22, align 8
  %24 = getelementptr i64, ptr %1, i32 12
  %25 = load i64, ptr %24, align 8
  %26 = alloca i64, i64 1, align 8
  store i64 0, ptr %26, align 8
  %27 = mul i64 %25, 1
  %28 = mul i64 %27, %25
  %29 = mul i64 %28, 8
  %30 = alloca i8, i64 %29, align 1
  %31 = alloca i8, i64 %29, align 1
  br label %32

32:                                               ; preds = %56, %4
  %33 = phi i64 [ 0, %4 ], [ %57, %56 ]
  %34 = icmp slt i64 %33, %25
  br i1 %34, label %35, label %58

35:                                               ; preds = %32
  br label %36

36:                                               ; preds = %39, %35
  %37 = phi i64 [ 0, %35 ], [ %55, %39 ]
  %38 = icmp slt i64 %37, %25
  br i1 %38, label %39, label %56

39:                                               ; preds = %36
  %40 = load i64, ptr %26, align 8
  %41 = mul i64 %40, 24
  %42 = trunc i64 %41 to i32
  %43 = getelementptr i8, ptr %3, i32 %42
  %44 = getelementptr { i64, i32, ptr }, ptr %43, i32 0, i32 0
  %45 = load i64, ptr %44, align 8
  %46 = getelementptr { i64, i32, ptr }, ptr %43, i32 0, i32 2
  %47 = load ptr, ptr %46, align 8
  %48 = mul i64 %37, 1
  %49 = add i64 %48, 0
  %50 = mul i64 %33, %27
  %51 = add i64 %49, %50
  %52 = getelementptr i64, ptr %30, i64 %51
  store i64 %45, ptr %52, align 8
  %53 = getelementptr ptr, ptr %31, i64 %51
  store ptr %47, ptr %53, align 8
  %54 = add i64 %40, 1
  store i64 %54, ptr %26, align 8
  %55 = add i64 %37, 1
  br label %36

56:                                               ; preds = %36
  %57 = add i64 %33, 1
  br label %32

58:                                               ; preds = %32
  %59 = alloca i8, i64 %29, align 1
  br label %60

60:                                               ; preds = %81, %58
  %61 = phi i64 [ 0, %58 ], [ %82, %81 ]
  %62 = icmp slt i64 %61, %25
  br i1 %62, label %63, label %83

63:                                               ; preds = %60
  br label %64

64:                                               ; preds = %67, %63
  %65 = phi i64 [ 0, %63 ], [ %80, %67 ]
  %66 = icmp slt i64 %65, %25
  br i1 %66, label %67, label %81

67:                                               ; preds = %64
  %68 = load i64, ptr %26, align 8
  %69 = mul i64 %68, 24
  %70 = trunc i64 %69 to i32
  %71 = getelementptr i8, ptr %3, i32 %70
  %72 = getelementptr { i64, i32, ptr }, ptr %71, i32 0, i32 2
  %73 = load ptr, ptr %72, align 8
  %74 = mul i64 %65, 1
  %75 = add i64 %74, 0
  %76 = mul i64 %61, %27
  %77 = add i64 %75, %76
  %78 = getelementptr ptr, ptr %59, i64 %77
  store ptr %73, ptr %78, align 8
  %79 = add i64 %68, 1
  store i64 %79, ptr %26, align 8
  %80 = add i64 %65, 1
  br label %64

81:                                               ; preds = %64
  %82 = add i64 %61, 1
  br label %60

83:                                               ; preds = %60
  %84 = alloca i8, i64 %29, align 1
  br label %85

85:                                               ; preds = %106, %83
  %86 = phi i64 [ 0, %83 ], [ %107, %106 ]
  %87 = icmp slt i64 %86, %25
  br i1 %87, label %88, label %108

88:                                               ; preds = %85
  br label %89

89:                                               ; preds = %92, %88
  %90 = phi i64 [ 0, %88 ], [ %105, %92 ]
  %91 = icmp slt i64 %90, %25
  br i1 %91, label %92, label %106

92:                                               ; preds = %89
  %93 = load i64, ptr %26, align 8
  %94 = mul i64 %93, 24
  %95 = trunc i64 %94 to i32
  %96 = getelementptr i8, ptr %3, i32 %95
  %97 = getelementptr { i64, i32, ptr }, ptr %96, i32 0, i32 2
  %98 = load ptr, ptr %97, align 8
  %99 = mul i64 %90, 1
  %100 = add i64 %99, 0
  %101 = mul i64 %86, %27
  %102 = add i64 %100, %101
  %103 = getelementptr ptr, ptr %84, i64 %102
  store ptr %98, ptr %103, align 8
  %104 = add i64 %93, 1
  store i64 %104, ptr %26, align 8
  %105 = add i64 %90, 1
  br label %89

106:                                              ; preds = %89
  %107 = add i64 %86, 1
  br label %85

108:                                              ; preds = %85
  br label %109

109:                                              ; preds = %144, %108
  %110 = phi i64 [ %21, %108 ], [ %145, %144 ]
  %111 = icmp slt i64 %110, %23
  br i1 %111, label %112, label %146

112:                                              ; preds = %109
  %113 = mul i64 %110, %27
  %114 = getelementptr ptr, ptr %84, i64 %113
  %115 = getelementptr ptr, ptr %31, i64 %113
  %116 = add i64 %7, %11
  br label %117

117:                                              ; preds = %142, %112
  %118 = phi i64 [ %19, %112 ], [ %143, %142 ]
  %119 = icmp slt i64 %118, %116
  br i1 %119, label %120, label %144

120:                                              ; preds = %117
  %121 = mul i64 %118, 8
  %122 = getelementptr i8, ptr %115, i64 %121
  %123 = add i64 %15, %11
  br label %124

124:                                              ; preds = %127, %120
  %125 = phi i64 [ %17, %120 ], [ %141, %127 ]
  %126 = icmp slt i64 %125, %123
  br i1 %126, label %127, label %142

127:                                              ; preds = %124
  %128 = mul i64 %125, 8
  %129 = getelementptr i8, ptr %114, i64 %128
  %130 = load ptr, ptr %129, align 8
  %131 = load float, ptr %130, align 4
  %132 = mul i64 %125, %27
  %133 = getelementptr ptr, ptr %59, i64 %132
  %134 = getelementptr i8, ptr %133, i64 %121
  %135 = load ptr, ptr %134, align 8
  %136 = load float, ptr %135, align 4
  %137 = fmul float %131, %136
  %138 = load ptr, ptr %122, align 8
  %139 = load float, ptr %138, align 4
  %140 = fadd float %139, %137
  store float %140, ptr %138, align 4
  %141 = add i64 %125, 1
  br label %124

142:                                              ; preds = %124
  %143 = add i64 %118, 1
  br label %117

144:                                              ; preds = %117
  %145 = add i64 %110, 1
  br label %109

146:                                              ; preds = %109
  %147 = alloca i64, i64 1, align 8
  store i64 13, ptr %147, align 8
  br label %148

148:                                              ; preds = %168, %146
  %149 = phi i64 [ 0, %146 ], [ %169, %168 ]
  %150 = icmp slt i64 %149, %25
  br i1 %150, label %151, label %170

151:                                              ; preds = %148
  br label %152

152:                                              ; preds = %155, %151
  %153 = phi i64 [ 0, %151 ], [ %167, %155 ]
  %154 = icmp slt i64 %153, %25
  br i1 %154, label %155, label %168

155:                                              ; preds = %152
  %156 = load i64, ptr %147, align 8
  %157 = trunc i64 %156 to i32
  %158 = getelementptr i64, ptr %1, i32 %157
  %159 = load i64, ptr %158, align 8
  %160 = mul i64 %153, 1
  %161 = add i64 %160, 0
  %162 = mul i64 %149, %27
  %163 = add i64 %161, %162
  %164 = getelementptr i64, ptr %30, i64 %163
  %165 = load i64, ptr %164, align 8
  call void @artsPersistentEventDecrementLatch(i64 %159, i64 %165)
  %166 = add i64 %156, 1
  store i64 %166, ptr %147, align 8
  %167 = add i64 %153, 1
  br label %152

168:                                              ; preds = %152
  %169 = add i64 %149, 1
  br label %148

170:                                              ; preds = %148
  ret void
}

define void @initPerWorker(i32 %0, i32 %1, ptr %2) {
  ret void
}

define void @initPerNode(i32 %0, i32 %1, ptr %2) {
  %4 = sext i32 %0 to i64
  %5 = icmp uge i64 %4, 1
  br i1 %5, label %6, label %7

6:                                                ; preds = %3
  ret void

7:                                                ; preds = %3
  %8 = call i32 @mainBody(i32 %1, ptr %2)
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

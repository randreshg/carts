; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str12 = internal constant [23 x i8] c"Results DO NOT match!\0A\00"
@str11 = internal constant [16 x i8] c"Results match!\0A\00"
@str10 = internal constant [50 x i8] c"Mismatch at (%d, %d): Parallel=%d, Sequential=%d\0A\00"
@str9 = internal constant [26 x i8] c"Sequential Score Matrix:\0A\00"
@str8 = internal constant [2 x i8] c"\0A\00"
@str7 = internal constant [4 x i8] c"%d \00"
@str6 = internal constant [24 x i8] c"Parallel Score Matrix:\0A\00"
@str5 = internal constant [22 x i8] c"Process has finished\0A\00"
@str4 = internal constant [29 x i8] c"Running task for i=%d, j=%d\0A\00"
@str3 = internal constant [10 x i8] c"seq2: %s\0A\00"
@str2 = internal constant [10 x i8] c"seq1: %s\0A\00"
@str1 = internal constant [8 x i8] c"TATGCGC\00"
@str0 = internal constant [9 x i8] c"AGTACGCA\00"

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

define i32 @mainBody() {
  %1 = call i32 @artsGetCurrentNode()
  %2 = alloca i64, i64 1, align 8
  %3 = call i64 @artsPersistentEventCreate(i32 %1, i32 0, i64 0)
  store i64 %3, ptr %2, align 8
  %4 = call i32 @artsGetCurrentNode()
  %5 = call i64 @artsReserveGuidRoute(i32 9, i32 %4)
  %6 = call ptr @artsDbCreateWithGuid(i64 %5, i64 8)
  %7 = call i32 @artsGetCurrentNode()
  %8 = alloca i64, i64 1, align 8
  %9 = call i64 @artsPersistentEventCreate(i32 %7, i32 0, i64 0)
  store i64 %9, ptr %8, align 8
  %10 = call i32 @artsGetCurrentNode()
  %11 = call i64 @artsReserveGuidRoute(i32 9, i32 %10)
  %12 = call ptr @artsDbCreateWithGuid(i64 %11, i64 9)
  %13 = load i8, ptr @str0, align 1
  store i8 %13, ptr %12, align 1
  %14 = load i8, ptr getelementptr inbounds ([9 x i8], ptr @str0, i32 0, i32 1), align 1
  %15 = getelementptr i8, ptr %12, i32 1
  store i8 %14, ptr %15, align 1
  %16 = load i8, ptr getelementptr inbounds ([9 x i8], ptr @str0, i32 0, i32 2), align 1
  %17 = getelementptr i8, ptr %12, i32 2
  store i8 %16, ptr %17, align 1
  %18 = load i8, ptr getelementptr inbounds ([9 x i8], ptr @str0, i32 0, i32 3), align 1
  %19 = getelementptr i8, ptr %12, i32 3
  store i8 %18, ptr %19, align 1
  %20 = load i8, ptr getelementptr inbounds ([9 x i8], ptr @str0, i32 0, i32 4), align 1
  %21 = getelementptr i8, ptr %12, i32 4
  store i8 %20, ptr %21, align 1
  %22 = load i8, ptr getelementptr inbounds ([9 x i8], ptr @str0, i32 0, i32 5), align 1
  %23 = getelementptr i8, ptr %12, i32 5
  store i8 %22, ptr %23, align 1
  %24 = load i8, ptr getelementptr inbounds ([9 x i8], ptr @str0, i32 0, i32 6), align 1
  %25 = getelementptr i8, ptr %12, i32 6
  store i8 %24, ptr %25, align 1
  %26 = load i8, ptr getelementptr inbounds ([9 x i8], ptr @str0, i32 0, i32 7), align 1
  %27 = getelementptr i8, ptr %12, i32 7
  store i8 %26, ptr %27, align 1
  %28 = load i8, ptr getelementptr inbounds ([9 x i8], ptr @str0, i32 0, i32 8), align 1
  %29 = getelementptr i8, ptr %12, i32 8
  store i8 %28, ptr %29, align 1
  %30 = load i8, ptr @str1, align 1
  store i8 %30, ptr %6, align 1
  %31 = load i8, ptr getelementptr inbounds ([8 x i8], ptr @str1, i32 0, i32 1), align 1
  %32 = getelementptr i8, ptr %6, i32 1
  store i8 %31, ptr %32, align 1
  %33 = load i8, ptr getelementptr inbounds ([8 x i8], ptr @str1, i32 0, i32 2), align 1
  %34 = getelementptr i8, ptr %6, i32 2
  store i8 %33, ptr %34, align 1
  %35 = load i8, ptr getelementptr inbounds ([8 x i8], ptr @str1, i32 0, i32 3), align 1
  %36 = getelementptr i8, ptr %6, i32 3
  store i8 %35, ptr %36, align 1
  %37 = load i8, ptr getelementptr inbounds ([8 x i8], ptr @str1, i32 0, i32 4), align 1
  %38 = getelementptr i8, ptr %6, i32 4
  store i8 %37, ptr %38, align 1
  %39 = load i8, ptr getelementptr inbounds ([8 x i8], ptr @str1, i32 0, i32 5), align 1
  %40 = getelementptr i8, ptr %6, i32 5
  store i8 %39, ptr %40, align 1
  %41 = load i8, ptr getelementptr inbounds ([8 x i8], ptr @str1, i32 0, i32 6), align 1
  %42 = getelementptr i8, ptr %6, i32 6
  store i8 %41, ptr %42, align 1
  %43 = load i8, ptr getelementptr inbounds ([8 x i8], ptr @str1, i32 0, i32 7), align 1
  %44 = getelementptr i8, ptr %6, i32 7
  store i8 %43, ptr %44, align 1
  %45 = call i64 @strlen(ptr %12)
  %46 = trunc i64 %45 to i32
  %47 = sext i32 %46 to i64
  %48 = call i64 @strlen(ptr %6)
  %49 = trunc i64 %48 to i32
  %50 = sext i32 %49 to i64
  %51 = add i32 %46, 1
  %52 = sext i32 %51 to i64
  %53 = add i32 %49, 1
  %54 = sext i32 %53 to i64
  %55 = call i32 @artsGetCurrentNode()
  %56 = mul i64 %52, 1
  %57 = mul i64 %56, %54
  %58 = mul i64 %57, 8
  %59 = alloca i8, i64 %58, align 1
  %60 = add i64 %47, 1
  br label %61

61:                                               ; preds = %78, %0
  %62 = phi i64 [ 0, %0 ], [ %79, %78 ]
  %63 = icmp slt i64 %62, %60
  br i1 %63, label %64, label %80

64:                                               ; preds = %61
  %65 = add i64 %50, 1
  br label %66

66:                                               ; preds = %69, %64
  %67 = phi i64 [ 0, %64 ], [ %77, %69 ]
  %68 = icmp slt i64 %67, %65
  br i1 %68, label %69, label %78

69:                                               ; preds = %66
  %70 = call i64 @artsPersistentEventCreate(i32 %55, i32 0, i64 0)
  %71 = mul i64 %67, 1
  %72 = add i64 %71, 0
  %73 = mul i64 %54, 1
  %74 = mul i64 %62, %73
  %75 = add i64 %72, %74
  %76 = getelementptr i64, ptr %59, i64 %75
  store i64 %70, ptr %76, align 8
  %77 = add i64 %67, 1
  br label %66

78:                                               ; preds = %66
  %79 = add i64 %62, 1
  br label %61

80:                                               ; preds = %61
  %81 = call i32 @artsGetCurrentNode()
  %82 = alloca i8, i64 %58, align 1
  %83 = alloca i8, i64 %58, align 1
  br label %84

84:                                               ; preds = %103, %80
  %85 = phi i64 [ 0, %80 ], [ %104, %103 ]
  %86 = icmp slt i64 %85, %60
  br i1 %86, label %87, label %105

87:                                               ; preds = %84
  %88 = add i64 %50, 1
  br label %89

89:                                               ; preds = %92, %87
  %90 = phi i64 [ 0, %87 ], [ %102, %92 ]
  %91 = icmp slt i64 %90, %88
  br i1 %91, label %92, label %103

92:                                               ; preds = %89
  %93 = call i64 @artsReserveGuidRoute(i32 9, i32 %81)
  %94 = call ptr @artsDbCreateWithGuid(i64 %93, i64 4)
  %95 = mul i64 %90, 1
  %96 = add i64 %95, 0
  %97 = mul i64 %54, 1
  %98 = mul i64 %85, %97
  %99 = add i64 %96, %98
  %100 = getelementptr i64, ptr %82, i64 %99
  store i64 %93, ptr %100, align 8
  %101 = getelementptr ptr, ptr %83, i64 %99
  store ptr %94, ptr %101, align 8
  %102 = add i64 %90, 1
  br label %89

103:                                              ; preds = %89
  %104 = add i64 %85, 1
  br label %84

105:                                              ; preds = %84
  %106 = mul i64 %57, 4
  %107 = alloca i8, i64 %106, align 1
  br label %108

108:                                              ; preds = %128, %105
  %109 = phi i64 [ 0, %105 ], [ %129, %128 ]
  %110 = icmp slt i64 %109, %60
  br i1 %110, label %111, label %130

111:                                              ; preds = %108
  %112 = mul i64 %54, 1
  %113 = mul i64 %109, %112
  %114 = getelementptr ptr, ptr %83, i64 %113
  %115 = add i64 %50, 1
  br label %116

116:                                              ; preds = %119, %111
  %117 = phi i64 [ 0, %111 ], [ %127, %119 ]
  %118 = icmp slt i64 %117, %115
  br i1 %118, label %119, label %128

119:                                              ; preds = %116
  %120 = mul i64 %117, 8
  %121 = getelementptr i8, ptr %114, i64 %120
  %122 = load ptr, ptr %121, align 8
  store i32 0, ptr %122, align 4
  %123 = mul i64 %117, 1
  %124 = add i64 %123, 0
  %125 = add i64 %124, %113
  %126 = getelementptr i32, ptr %107, i64 %125
  store i32 0, ptr %126, align 4
  %127 = add i64 %117, 1
  br label %116

128:                                              ; preds = %116
  %129 = add i64 %109, 1
  br label %108

130:                                              ; preds = %108
  %131 = call i32 @artsGetCurrentNode()
  %132 = alloca i64, i64 0, align 8
  %133 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %131, i32 0, ptr %132, i32 1)
  %134 = call i64 @artsInitializeAndStartEpoch(i64 %133, i32 0)
  %135 = call i32 @artsGetCurrentNode()
  %136 = mul i64 %52, %54
  %137 = add i64 %136, 1
  %138 = add i64 %136, 2
  %139 = trunc i64 %138 to i32
  %140 = alloca i64, i64 6, align 8
  store i64 %52, ptr %140, align 8
  %141 = getelementptr i64, ptr %140, i32 1
  store i64 %54, ptr %141, align 8
  %142 = getelementptr i64, ptr %140, i32 2
  store i64 %50, ptr %142, align 8
  %143 = getelementptr i64, ptr %140, i32 3
  store i64 %52, ptr %143, align 8
  %144 = getelementptr i64, ptr %140, i32 4
  store i64 %52, ptr %144, align 8
  %145 = getelementptr i64, ptr %140, i32 5
  store i64 %54, ptr %145, align 8
  %146 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %135, i32 6, ptr %140, i32 %139, i64 %134)
  %147 = load i64, ptr %8, align 8
  call void @artsAddDependenceToPersistentEvent(i64 %147, i64 %146, i32 0, i64 %11)
  %148 = call ptr @malloc(i64 8)
  store i64 1, ptr %148, align 8
  br label %149

149:                                              ; preds = %171, %130
  %150 = phi i64 [ 0, %130 ], [ %172, %171 ]
  %151 = icmp slt i64 %150, %60
  br i1 %151, label %152, label %173

152:                                              ; preds = %149
  %153 = add i64 %50, 1
  br label %154

154:                                              ; preds = %157, %152
  %155 = phi i64 [ 0, %152 ], [ %170, %157 ]
  %156 = icmp slt i64 %155, %153
  br i1 %156, label %157, label %171

157:                                              ; preds = %154
  %158 = mul i64 %155, 1
  %159 = add i64 %158, 0
  %160 = mul i64 %54, 1
  %161 = mul i64 %150, %160
  %162 = add i64 %159, %161
  %163 = getelementptr i64, ptr %59, i64 %162
  %164 = load i64, ptr %163, align 8
  %165 = load i64, ptr %148, align 8
  %166 = getelementptr i64, ptr %82, i64 %162
  %167 = load i64, ptr %166, align 8
  %168 = trunc i64 %165 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %164, i64 %146, i32 %168, i64 %167)
  %169 = add i64 %165, 1
  store i64 %169, ptr %148, align 8
  %170 = add i64 %155, 1
  br label %154

171:                                              ; preds = %154
  %172 = add i64 %150, 1
  br label %149

173:                                              ; preds = %149
  %174 = load i64, ptr %2, align 8
  %175 = trunc i64 %137 to i32
  call void @artsAddDependenceToPersistentEvent(i64 %174, i64 %146, i32 %175, i64 %5)
  %176 = call i1 @artsWaitOnHandle(i64 %134)
  %177 = call i32 (ptr, ...) @printf(ptr @str5)
  %178 = call i32 (ptr, ...) @printf(ptr @str6)
  br label %179

179:                                              ; preds = %197, %173
  %180 = phi i64 [ 0, %173 ], [ %199, %197 ]
  %181 = icmp slt i64 %180, %60
  br i1 %181, label %182, label %200

182:                                              ; preds = %179
  %183 = mul i64 %54, 1
  %184 = mul i64 %180, %183
  %185 = getelementptr ptr, ptr %83, i64 %184
  %186 = add i64 %50, 1
  br label %187

187:                                              ; preds = %190, %182
  %188 = phi i64 [ 0, %182 ], [ %196, %190 ]
  %189 = icmp slt i64 %188, %186
  br i1 %189, label %190, label %197

190:                                              ; preds = %187
  %191 = mul i64 %188, 8
  %192 = getelementptr i8, ptr %185, i64 %191
  %193 = load ptr, ptr %192, align 8
  %194 = load i32, ptr %193, align 4
  %195 = call i32 (ptr, ...) @printf(ptr @str7, i32 %194)
  %196 = add i64 %188, 1
  br label %187

197:                                              ; preds = %187
  %198 = call i32 (ptr, ...) @printf(ptr @str8)
  %199 = add i64 %180, 1
  br label %179

200:                                              ; preds = %179
  br label %201

201:                                              ; preds = %258, %200
  %202 = phi i64 [ 1, %200 ], [ %259, %258 ]
  %203 = icmp slt i64 %202, %60
  br i1 %203, label %204, label %260

204:                                              ; preds = %201
  %205 = trunc i64 %202 to i32
  %206 = add i32 %205, -1
  %207 = sext i32 %206 to i64
  %208 = getelementptr i8, ptr %12, i64 %207
  %209 = add i64 %50, 1
  br label %210

210:                                              ; preds = %256, %204
  %211 = phi i64 [ 1, %204 ], [ %257, %256 ]
  %212 = icmp slt i64 %211, %209
  br i1 %212, label %213, label %258

213:                                              ; preds = %210
  %214 = trunc i64 %211 to i32
  %215 = add i32 %214, -1
  %216 = sext i32 %215 to i64
  %217 = add i64 %202, -1
  %218 = add i64 %211, -1
  %219 = mul i64 %218, 1
  %220 = add i64 %219, 0
  %221 = mul i64 %54, 1
  %222 = mul i64 %217, %221
  %223 = add i64 %220, %222
  %224 = getelementptr i32, ptr %107, i64 %223
  %225 = load i32, ptr %224, align 4
  %226 = load i8, ptr %208, align 1
  %227 = getelementptr i8, ptr %6, i64 %216
  %228 = load i8, ptr %227, align 1
  %229 = icmp eq i8 %226, %228
  %230 = select i1 %229, i32 2, i32 -1
  %231 = add i32 %225, %230
  %232 = mul i64 %211, 1
  %233 = add i64 %232, 0
  %234 = add i64 %233, %222
  %235 = getelementptr i32, ptr %107, i64 %234
  %236 = load i32, ptr %235, align 4
  %237 = add i32 %236, -2
  %238 = mul i64 %202, %221
  %239 = add i64 %220, %238
  %240 = getelementptr i32, ptr %107, i64 %239
  %241 = load i32, ptr %240, align 4
  %242 = add i32 %241, -2
  %243 = icmp sgt i32 %231, %237
  br i1 %243, label %244, label %324

244:                                              ; preds = %324, %213
  %245 = phi i32 [ %325, %324 ], [ %231, %213 ]
  %246 = phi i32 [ %326, %324 ], [ %231, %213 ]
  %247 = icmp sgt i32 %245, %242
  %248 = select i1 %247, i32 %246, i32 %242
  br label %249

249:                                              ; preds = %244
  %250 = phi i32 [ %248, %244 ]
  br label %251

251:                                              ; preds = %249
  %252 = add i64 %233, %238
  %253 = getelementptr i32, ptr %107, i64 %252
  store i32 %250, ptr %253, align 4
  %254 = icmp slt i32 %250, 0
  br i1 %254, label %255, label %256

255:                                              ; preds = %251
  store i32 0, ptr %253, align 4
  br label %256

256:                                              ; preds = %255, %251
  %257 = add i64 %211, 1
  br label %210

258:                                              ; preds = %210
  %259 = add i64 %202, 1
  br label %201

260:                                              ; preds = %201
  %261 = call i32 (ptr, ...) @printf(ptr @str9)
  br label %262

262:                                              ; preds = %280, %260
  %263 = phi i64 [ 0, %260 ], [ %282, %280 ]
  %264 = icmp slt i64 %263, %60
  br i1 %264, label %265, label %283

265:                                              ; preds = %262
  %266 = add i64 %50, 1
  br label %267

267:                                              ; preds = %270, %265
  %268 = phi i64 [ 0, %265 ], [ %279, %270 ]
  %269 = icmp slt i64 %268, %266
  br i1 %269, label %270, label %280

270:                                              ; preds = %267
  %271 = mul i64 %268, 1
  %272 = add i64 %271, 0
  %273 = mul i64 %54, 1
  %274 = mul i64 %263, %273
  %275 = add i64 %272, %274
  %276 = getelementptr i32, ptr %107, i64 %275
  %277 = load i32, ptr %276, align 4
  %278 = call i32 (ptr, ...) @printf(ptr @str7, i32 %277)
  %279 = add i64 %268, 1
  br label %267

280:                                              ; preds = %267
  %281 = call i32 (ptr, ...) @printf(ptr @str8)
  %282 = add i64 %263, 1
  br label %262

283:                                              ; preds = %262
  br label %284

284:                                              ; preds = %315, %283
  %285 = phi i64 [ 0, %283 ], [ %316, %315 ]
  %286 = phi i32 [ 1, %283 ], [ %296, %315 ]
  %287 = icmp slt i64 %285, %60
  br i1 %287, label %288, label %317

288:                                              ; preds = %284
  %289 = trunc i64 %285 to i32
  %290 = mul i64 %54, 1
  %291 = mul i64 %285, %290
  %292 = getelementptr ptr, ptr %83, i64 %291
  %293 = add i64 %50, 1
  br label %294

294:                                              ; preds = %313, %288
  %295 = phi i64 [ 0, %288 ], [ %314, %313 ]
  %296 = phi i32 [ %286, %288 ], [ %310, %313 ]
  %297 = icmp slt i64 %295, %293
  br i1 %297, label %298, label %315

298:                                              ; preds = %294
  %299 = trunc i64 %295 to i32
  %300 = mul i64 %295, 8
  %301 = getelementptr i8, ptr %292, i64 %300
  %302 = load ptr, ptr %301, align 8
  %303 = load i32, ptr %302, align 4
  %304 = mul i64 %295, 1
  %305 = add i64 %304, 0
  %306 = add i64 %305, %291
  %307 = getelementptr i32, ptr %107, i64 %306
  %308 = load i32, ptr %307, align 4
  %309 = icmp ne i32 %303, %308
  %310 = select i1 %309, i32 0, i32 %296
  br i1 %309, label %311, label %313

311:                                              ; preds = %298
  %312 = call i32 (ptr, ...) @printf(ptr @str10, i32 %289, i32 %299, i32 %303, i32 %308)
  br label %313

313:                                              ; preds = %311, %298
  %314 = add i64 %295, 1
  br label %294

315:                                              ; preds = %294
  %316 = add i64 %285, 1
  br label %284

317:                                              ; preds = %284
  %318 = icmp ne i32 %286, 0
  br i1 %318, label %319, label %321

319:                                              ; preds = %317
  %320 = call i32 (ptr, ...) @printf(ptr @str11)
  br label %323

321:                                              ; preds = %317
  %322 = call i32 (ptr, ...) @printf(ptr @str12)
  br label %323

323:                                              ; preds = %319, %321
  ret i32 0

324:                                              ; preds = %213
  %325 = phi i32 [ %237, %213 ]
  %326 = phi i32 [ %237, %213 ]
  br label %244
}

declare i64 @strlen(ptr)

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = getelementptr i64, ptr %1, i32 2
  %6 = load i64, ptr %5, align 8
  %7 = trunc i64 %6 to i32
  %8 = sext i32 %7 to i64
  %9 = getelementptr i64, ptr %1, i32 3
  %10 = load i64, ptr %9, align 8
  %11 = getelementptr i64, ptr %1, i32 4
  %12 = load i64, ptr %11, align 8
  %13 = getelementptr i64, ptr %1, i32 5
  %14 = load i64, ptr %13, align 8
  %15 = alloca i64, i64 1, align 8
  store i64 0, ptr %15, align 8
  %16 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %17 = load ptr, ptr %16, align 8
  store i64 1, ptr %15, align 8
  %18 = mul i64 %12, 1
  %19 = mul i64 %18, %14
  %20 = mul i64 %19, 8
  %21 = alloca i8, i64 %20, align 1
  br label %22

22:                                               ; preds = %44, %4
  %23 = phi i64 [ 0, %4 ], [ %45, %44 ]
  %24 = icmp slt i64 %23, %12
  br i1 %24, label %25, label %46

25:                                               ; preds = %22
  br label %26

26:                                               ; preds = %29, %25
  %27 = phi i64 [ 0, %25 ], [ %43, %29 ]
  %28 = icmp slt i64 %27, %14
  br i1 %28, label %29, label %44

29:                                               ; preds = %26
  %30 = load i64, ptr %15, align 8
  %31 = mul i64 %30, 24
  %32 = trunc i64 %31 to i32
  %33 = getelementptr i8, ptr %3, i32 %32
  %34 = getelementptr { i64, i32, ptr }, ptr %33, i32 0, i32 0
  %35 = load i64, ptr %34, align 8
  %36 = mul i64 %27, 1
  %37 = add i64 %36, 0
  %38 = mul i64 %14, 1
  %39 = mul i64 %23, %38
  %40 = add i64 %37, %39
  %41 = getelementptr i64, ptr %21, i64 %40
  store i64 %35, ptr %41, align 8
  %42 = add i64 %30, 1
  store i64 %42, ptr %15, align 8
  %43 = add i64 %27, 1
  br label %26

44:                                               ; preds = %26
  %45 = add i64 %23, 1
  br label %22

46:                                               ; preds = %22
  %47 = load i64, ptr %15, align 8
  %48 = mul i64 %47, 24
  %49 = trunc i64 %48 to i32
  %50 = getelementptr i8, ptr %3, i32 %49
  %51 = getelementptr { i64, i32, ptr }, ptr %50, i32 0, i32 2
  %52 = load ptr, ptr %51, align 8
  %53 = add i64 %47, 1
  store i64 %53, ptr %15, align 8
  %54 = call i32 @artsGetCurrentNode()
  %55 = alloca i8, i64 %20, align 1
  br label %56

56:                                               ; preds = %72, %46
  %57 = phi i64 [ 0, %46 ], [ %73, %72 ]
  %58 = icmp slt i64 %57, %12
  br i1 %58, label %59, label %74

59:                                               ; preds = %56
  br label %60

60:                                               ; preds = %63, %59
  %61 = phi i64 [ 0, %59 ], [ %71, %63 ]
  %62 = icmp slt i64 %61, %14
  br i1 %62, label %63, label %72

63:                                               ; preds = %60
  %64 = call i64 @artsPersistentEventCreate(i32 %54, i32 0, i64 0)
  %65 = mul i64 %61, 1
  %66 = add i64 %65, 0
  %67 = mul i64 %14, 1
  %68 = mul i64 %57, %67
  %69 = add i64 %66, %68
  %70 = getelementptr i64, ptr %55, i64 %69
  store i64 %64, ptr %70, align 8
  %71 = add i64 %61, 1
  br label %60

72:                                               ; preds = %60
  %73 = add i64 %57, 1
  br label %56

74:                                               ; preds = %56
  %75 = call i32 (ptr, ...) @printf(ptr @str2, ptr %17)
  %76 = call i32 (ptr, ...) @printf(ptr @str3, ptr %52)
  %77 = alloca i64, i64 5, align 8
  br label %78

78:                                               ; preds = %141, %74
  %79 = phi i64 [ 1, %74 ], [ %142, %141 ]
  %80 = icmp slt i64 %79, %10
  br i1 %80, label %81, label %143

81:                                               ; preds = %78
  %82 = trunc i64 %79 to i32
  %83 = add i32 %82, -1
  %84 = sext i32 %83 to i64
  %85 = getelementptr i8, ptr %17, i64 %84
  %86 = sext i32 %82 to i64
  %87 = add i64 %8, 1
  br label %88

88:                                               ; preds = %91, %81
  %89 = phi i64 [ 1, %81 ], [ %140, %91 ]
  %90 = icmp slt i64 %89, %87
  br i1 %90, label %91, label %141

91:                                               ; preds = %88
  %92 = trunc i64 %89 to i32
  %93 = add i32 %92, -1
  %94 = sext i32 %93 to i64
  %95 = load i8, ptr %85, align 1
  %96 = getelementptr i8, ptr %52, i64 %94
  %97 = load i8, ptr %96, align 1
  %98 = call i64 @artsGetCurrentEpochGuid()
  %99 = call i32 @artsGetCurrentNode()
  store i64 %86, ptr %77, align 8
  %100 = sext i32 %92 to i64
  %101 = getelementptr i64, ptr %77, i32 1
  store i64 %100, ptr %101, align 8
  %102 = sext i8 %95 to i64
  %103 = getelementptr i64, ptr %77, i32 2
  store i64 %102, ptr %103, align 8
  %104 = sext i8 %97 to i64
  %105 = getelementptr i64, ptr %77, i32 3
  store i64 %104, ptr %105, align 8
  %106 = mul i64 %89, 1
  %107 = add i64 %106, 0
  %108 = mul i64 %14, 1
  %109 = mul i64 %79, %108
  %110 = add i64 %107, %109
  %111 = getelementptr i64, ptr %55, i64 %110
  %112 = load i64, ptr %111, align 8
  %113 = getelementptr i64, ptr %77, i32 4
  store i64 %112, ptr %113, align 8
  %114 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %99, i32 5, ptr %77, i32 4, i64 %98)
  %115 = add i64 %89, -1
  %116 = mul i64 %115, 1
  %117 = add i64 %116, 0
  %118 = add i64 %117, %109
  %119 = getelementptr i64, ptr %55, i64 %118
  %120 = load i64, ptr %119, align 8
  %121 = getelementptr i64, ptr %21, i64 %118
  %122 = load i64, ptr %121, align 8
  call void @artsAddDependenceToPersistentEvent(i64 %120, i64 %114, i32 0, i64 %122)
  %123 = add i64 %79, -1
  %124 = mul i64 %123, %108
  %125 = add i64 %107, %124
  %126 = getelementptr i64, ptr %55, i64 %125
  %127 = load i64, ptr %126, align 8
  %128 = getelementptr i64, ptr %21, i64 %125
  %129 = load i64, ptr %128, align 8
  call void @artsAddDependenceToPersistentEvent(i64 %127, i64 %114, i32 1, i64 %129)
  %130 = load i64, ptr %111, align 8
  %131 = getelementptr i64, ptr %21, i64 %110
  %132 = load i64, ptr %131, align 8
  call void @artsAddDependenceToPersistentEvent(i64 %130, i64 %114, i32 2, i64 %132)
  %133 = add i64 %117, %124
  %134 = getelementptr i64, ptr %55, i64 %133
  %135 = load i64, ptr %134, align 8
  %136 = getelementptr i64, ptr %21, i64 %133
  %137 = load i64, ptr %136, align 8
  call void @artsAddDependenceToPersistentEvent(i64 %135, i64 %114, i32 3, i64 %137)
  %138 = load i64, ptr %111, align 8
  %139 = load i64, ptr %131, align 8
  call void @artsPersistentEventIncrementLatch(i64 %138, i64 %139)
  %140 = add i64 %89, 1
  br label %88

141:                                              ; preds = %88
  %142 = add i64 %79, 1
  br label %78

143:                                              ; preds = %78
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = load i64, ptr %1, align 8
  %6 = trunc i64 %5 to i32
  %7 = getelementptr i64, ptr %1, i32 1
  %8 = load i64, ptr %7, align 8
  %9 = trunc i64 %8 to i32
  %10 = getelementptr i64, ptr %1, i32 2
  %11 = load i64, ptr %10, align 8
  %12 = trunc i64 %11 to i8
  %13 = getelementptr i64, ptr %1, i32 3
  %14 = load i64, ptr %13, align 8
  %15 = trunc i64 %14 to i8
  %16 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %17 = load ptr, ptr %16, align 8
  %18 = getelementptr i8, ptr %3, i32 24
  %19 = getelementptr { i64, i32, ptr }, ptr %18, i32 0, i32 2
  %20 = load ptr, ptr %19, align 8
  %21 = getelementptr i8, ptr %3, i32 48
  %22 = getelementptr { i64, i32, ptr }, ptr %21, i32 0, i32 0
  %23 = load i64, ptr %22, align 8
  %24 = getelementptr { i64, i32, ptr }, ptr %21, i32 0, i32 2
  %25 = load ptr, ptr %24, align 8
  %26 = getelementptr i8, ptr %3, i32 72
  %27 = getelementptr { i64, i32, ptr }, ptr %26, i32 0, i32 2
  %28 = load ptr, ptr %27, align 8
  %29 = call i32 (ptr, ...) @printf(ptr @str4, i32 %6, i32 %9)
  %30 = load i32, ptr %28, align 4
  %31 = icmp eq i8 %12, %15
  %32 = select i1 %31, i32 2, i32 -1
  %33 = add i32 %30, %32
  %34 = load i32, ptr %20, align 4
  %35 = add i32 %34, -2
  %36 = load i32, ptr %17, align 4
  %37 = add i32 %36, -2
  %38 = icmp sgt i32 %33, %35
  br i1 %38, label %39, label %53

39:                                               ; preds = %53, %4
  %40 = phi i32 [ %54, %53 ], [ %33, %4 ]
  %41 = phi i32 [ %55, %53 ], [ %33, %4 ]
  %42 = icmp sgt i32 %40, %37
  %43 = select i1 %42, i32 %41, i32 %37
  br label %44

44:                                               ; preds = %39
  %45 = phi i32 [ %43, %39 ]
  br label %46

46:                                               ; preds = %44
  store i32 %45, ptr %25, align 4
  %47 = load i32, ptr %25, align 4
  %48 = icmp slt i32 %47, 0
  br i1 %48, label %49, label %50

49:                                               ; preds = %46
  store i32 0, ptr %25, align 4
  br label %50

50:                                               ; preds = %49, %46
  %51 = getelementptr i64, ptr %1, i32 4
  %52 = load i64, ptr %51, align 8
  call void @artsPersistentEventDecrementLatch(i64 %52, i64 %23)
  ret void

53:                                               ; preds = %4
  %54 = phi i32 [ %35, %4 ]
  %55 = phi i32 [ %35, %4 ]
  br label %39
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
  %8 = call i32 @mainBody()
  call void @artsShutdown()
  ret void
}

define i32 @main(i32 %0, ptr %1) {
  %3 = call i32 @artsRT(i32 %0, ptr %1)
  ret i32 0
}

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}

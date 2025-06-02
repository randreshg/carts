; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str7 = internal constant [19 x i8] c"Result: INCORRECT\0A\00"
@str6 = internal constant [17 x i8] c"Result: CORRECT\0A\00"
@str5 = internal constant [22 x i8] c"Process has finished\0A\00"
@str4 = internal constant [24 x i8] c"Finished in %f seconds\0A\00"
@str3 = internal constant [10 x i8] c"seq2: %s\0A\00"
@str2 = internal constant [10 x i8] c"seq1: %s\0A\00"
@str1 = internal constant [33 x i8] c"TATGCGCTAGCTAGGCTATGCGATCGTAGCGA\00"
@str0 = internal constant [33 x i8] c"AGTACGCATGACCTGATCGTACGATCGATGCA\00"

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
  %1 = call i32 @artsGetCurrentNode()
  %2 = call i64 @artsReserveGuidRoute(i32 8, i32 %1)
  %3 = call ptr @artsDbCreateWithGuid(i64 %2, i64 33)
  %4 = call i32 @artsGetCurrentNode()
  %5 = call i64 @artsReserveGuidRoute(i32 8, i32 %4)
  %6 = call ptr @artsDbCreateWithGuid(i64 %5, i64 33)
  %7 = load i8, ptr @str0, align 1
  store i8 %7, ptr %6, align 1
  %8 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 1), align 1
  %9 = getelementptr i8, ptr %6, i32 1
  store i8 %8, ptr %9, align 1
  %10 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 2), align 1
  %11 = getelementptr i8, ptr %6, i32 2
  store i8 %10, ptr %11, align 1
  %12 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 3), align 1
  %13 = getelementptr i8, ptr %6, i32 3
  store i8 %12, ptr %13, align 1
  %14 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 4), align 1
  %15 = getelementptr i8, ptr %6, i32 4
  store i8 %14, ptr %15, align 1
  %16 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 5), align 1
  %17 = getelementptr i8, ptr %6, i32 5
  store i8 %16, ptr %17, align 1
  %18 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 6), align 1
  %19 = getelementptr i8, ptr %6, i32 6
  store i8 %18, ptr %19, align 1
  %20 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 7), align 1
  %21 = getelementptr i8, ptr %6, i32 7
  store i8 %20, ptr %21, align 1
  %22 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 8), align 1
  %23 = getelementptr i8, ptr %6, i32 8
  store i8 %22, ptr %23, align 1
  %24 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 9), align 1
  %25 = getelementptr i8, ptr %6, i32 9
  store i8 %24, ptr %25, align 1
  %26 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 10), align 1
  %27 = getelementptr i8, ptr %6, i32 10
  store i8 %26, ptr %27, align 1
  %28 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 11), align 1
  %29 = getelementptr i8, ptr %6, i32 11
  store i8 %28, ptr %29, align 1
  %30 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 12), align 1
  %31 = getelementptr i8, ptr %6, i32 12
  store i8 %30, ptr %31, align 1
  %32 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 13), align 1
  %33 = getelementptr i8, ptr %6, i32 13
  store i8 %32, ptr %33, align 1
  %34 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 14), align 1
  %35 = getelementptr i8, ptr %6, i32 14
  store i8 %34, ptr %35, align 1
  %36 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 15), align 1
  %37 = getelementptr i8, ptr %6, i32 15
  store i8 %36, ptr %37, align 1
  %38 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 16), align 1
  %39 = getelementptr i8, ptr %6, i32 16
  store i8 %38, ptr %39, align 1
  %40 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 17), align 1
  %41 = getelementptr i8, ptr %6, i32 17
  store i8 %40, ptr %41, align 1
  %42 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 18), align 1
  %43 = getelementptr i8, ptr %6, i32 18
  store i8 %42, ptr %43, align 1
  %44 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 19), align 1
  %45 = getelementptr i8, ptr %6, i32 19
  store i8 %44, ptr %45, align 1
  %46 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 20), align 1
  %47 = getelementptr i8, ptr %6, i32 20
  store i8 %46, ptr %47, align 1
  %48 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 21), align 1
  %49 = getelementptr i8, ptr %6, i32 21
  store i8 %48, ptr %49, align 1
  %50 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 22), align 1
  %51 = getelementptr i8, ptr %6, i32 22
  store i8 %50, ptr %51, align 1
  %52 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 23), align 1
  %53 = getelementptr i8, ptr %6, i32 23
  store i8 %52, ptr %53, align 1
  %54 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 24), align 1
  %55 = getelementptr i8, ptr %6, i32 24
  store i8 %54, ptr %55, align 1
  %56 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 25), align 1
  %57 = getelementptr i8, ptr %6, i32 25
  store i8 %56, ptr %57, align 1
  %58 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 26), align 1
  %59 = getelementptr i8, ptr %6, i32 26
  store i8 %58, ptr %59, align 1
  %60 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 27), align 1
  %61 = getelementptr i8, ptr %6, i32 27
  store i8 %60, ptr %61, align 1
  %62 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 28), align 1
  %63 = getelementptr i8, ptr %6, i32 28
  store i8 %62, ptr %63, align 1
  %64 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 29), align 1
  %65 = getelementptr i8, ptr %6, i32 29
  store i8 %64, ptr %65, align 1
  %66 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 30), align 1
  %67 = getelementptr i8, ptr %6, i32 30
  store i8 %66, ptr %67, align 1
  %68 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 31), align 1
  %69 = getelementptr i8, ptr %6, i32 31
  store i8 %68, ptr %69, align 1
  %70 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str0, i32 0, i32 32), align 1
  %71 = getelementptr i8, ptr %6, i32 32
  store i8 %70, ptr %71, align 1
  %72 = load i8, ptr @str1, align 1
  store i8 %72, ptr %3, align 1
  %73 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 1), align 1
  %74 = getelementptr i8, ptr %3, i32 1
  store i8 %73, ptr %74, align 1
  %75 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 2), align 1
  %76 = getelementptr i8, ptr %3, i32 2
  store i8 %75, ptr %76, align 1
  %77 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 3), align 1
  %78 = getelementptr i8, ptr %3, i32 3
  store i8 %77, ptr %78, align 1
  %79 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 4), align 1
  %80 = getelementptr i8, ptr %3, i32 4
  store i8 %79, ptr %80, align 1
  %81 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 5), align 1
  %82 = getelementptr i8, ptr %3, i32 5
  store i8 %81, ptr %82, align 1
  %83 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 6), align 1
  %84 = getelementptr i8, ptr %3, i32 6
  store i8 %83, ptr %84, align 1
  %85 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 7), align 1
  %86 = getelementptr i8, ptr %3, i32 7
  store i8 %85, ptr %86, align 1
  %87 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 8), align 1
  %88 = getelementptr i8, ptr %3, i32 8
  store i8 %87, ptr %88, align 1
  %89 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 9), align 1
  %90 = getelementptr i8, ptr %3, i32 9
  store i8 %89, ptr %90, align 1
  %91 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 10), align 1
  %92 = getelementptr i8, ptr %3, i32 10
  store i8 %91, ptr %92, align 1
  %93 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 11), align 1
  %94 = getelementptr i8, ptr %3, i32 11
  store i8 %93, ptr %94, align 1
  %95 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 12), align 1
  %96 = getelementptr i8, ptr %3, i32 12
  store i8 %95, ptr %96, align 1
  %97 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 13), align 1
  %98 = getelementptr i8, ptr %3, i32 13
  store i8 %97, ptr %98, align 1
  %99 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 14), align 1
  %100 = getelementptr i8, ptr %3, i32 14
  store i8 %99, ptr %100, align 1
  %101 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 15), align 1
  %102 = getelementptr i8, ptr %3, i32 15
  store i8 %101, ptr %102, align 1
  %103 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 16), align 1
  %104 = getelementptr i8, ptr %3, i32 16
  store i8 %103, ptr %104, align 1
  %105 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 17), align 1
  %106 = getelementptr i8, ptr %3, i32 17
  store i8 %105, ptr %106, align 1
  %107 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 18), align 1
  %108 = getelementptr i8, ptr %3, i32 18
  store i8 %107, ptr %108, align 1
  %109 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 19), align 1
  %110 = getelementptr i8, ptr %3, i32 19
  store i8 %109, ptr %110, align 1
  %111 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 20), align 1
  %112 = getelementptr i8, ptr %3, i32 20
  store i8 %111, ptr %112, align 1
  %113 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 21), align 1
  %114 = getelementptr i8, ptr %3, i32 21
  store i8 %113, ptr %114, align 1
  %115 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 22), align 1
  %116 = getelementptr i8, ptr %3, i32 22
  store i8 %115, ptr %116, align 1
  %117 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 23), align 1
  %118 = getelementptr i8, ptr %3, i32 23
  store i8 %117, ptr %118, align 1
  %119 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 24), align 1
  %120 = getelementptr i8, ptr %3, i32 24
  store i8 %119, ptr %120, align 1
  %121 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 25), align 1
  %122 = getelementptr i8, ptr %3, i32 25
  store i8 %121, ptr %122, align 1
  %123 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 26), align 1
  %124 = getelementptr i8, ptr %3, i32 26
  store i8 %123, ptr %124, align 1
  %125 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 27), align 1
  %126 = getelementptr i8, ptr %3, i32 27
  store i8 %125, ptr %126, align 1
  %127 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 28), align 1
  %128 = getelementptr i8, ptr %3, i32 28
  store i8 %127, ptr %128, align 1
  %129 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 29), align 1
  %130 = getelementptr i8, ptr %3, i32 29
  store i8 %129, ptr %130, align 1
  %131 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 30), align 1
  %132 = getelementptr i8, ptr %3, i32 30
  store i8 %131, ptr %132, align 1
  %133 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 31), align 1
  %134 = getelementptr i8, ptr %3, i32 31
  store i8 %133, ptr %134, align 1
  %135 = load i8, ptr getelementptr inbounds ([33 x i8], ptr @str1, i32 0, i32 32), align 1
  %136 = getelementptr i8, ptr %3, i32 32
  store i8 %135, ptr %136, align 1
  %137 = call i64 @strlen(ptr %6)
  %138 = trunc i64 %137 to i32
  %139 = call i64 @strlen(ptr %3)
  %140 = trunc i64 %139 to i32
  %141 = add i32 %138, 1
  %142 = sext i32 %141 to i64
  %143 = add i32 %140, 1
  %144 = sext i32 %143 to i64
  %145 = call i32 @artsGetCurrentNode()
  %146 = mul i64 %142, 1
  %147 = mul i64 %146, %144
  %148 = mul i64 %147, 8
  %149 = alloca i8, i64 %148, align 1
  %150 = alloca i8, i64 %148, align 1
  br label %151

151:                                              ; preds = %169, %0
  %152 = phi i64 [ 0, %0 ], [ %170, %169 ]
  %153 = icmp slt i64 %152, %142
  br i1 %153, label %154, label %171

154:                                              ; preds = %151
  br label %155

155:                                              ; preds = %158, %154
  %156 = phi i64 [ 0, %154 ], [ %168, %158 ]
  %157 = icmp slt i64 %156, %144
  br i1 %157, label %158, label %169

158:                                              ; preds = %155
  %159 = call i64 @artsReserveGuidRoute(i32 10, i32 %145)
  %160 = call ptr @artsDbCreateWithGuid(i64 %159, i64 4)
  %161 = mul i64 %156, 1
  %162 = add i64 %161, 0
  %163 = mul i64 %144, 1
  %164 = mul i64 %152, %163
  %165 = add i64 %162, %164
  %166 = getelementptr i64, ptr %149, i64 %165
  store i64 %159, ptr %166, align 8
  %167 = getelementptr ptr, ptr %150, i64 %165
  store ptr %160, ptr %167, align 8
  %168 = add i64 %156, 1
  br label %155

169:                                              ; preds = %155
  %170 = add i64 %152, 1
  br label %151

171:                                              ; preds = %151
  %172 = mul i64 %147, 4
  %173 = alloca i8, i64 %172, align 1
  br label %174

174:                                              ; preds = %193, %171
  %175 = phi i64 [ 0, %171 ], [ %194, %193 ]
  %176 = icmp slt i64 %175, %142
  br i1 %176, label %177, label %195

177:                                              ; preds = %174
  %178 = mul i64 %144, 1
  %179 = mul i64 %175, %178
  %180 = getelementptr ptr, ptr %150, i64 %179
  br label %181

181:                                              ; preds = %184, %177
  %182 = phi i64 [ 0, %177 ], [ %192, %184 ]
  %183 = icmp slt i64 %182, %144
  br i1 %183, label %184, label %193

184:                                              ; preds = %181
  %185 = mul i64 %182, 8
  %186 = getelementptr i8, ptr %180, i64 %185
  %187 = load ptr, ptr %186, align 8
  store i32 0, ptr %187, align 4
  %188 = mul i64 %182, 1
  %189 = add i64 %188, 0
  %190 = add i64 %189, %179
  %191 = getelementptr i32, ptr %173, i64 %190
  store i32 0, ptr %191, align 4
  %192 = add i64 %182, 1
  br label %181

193:                                              ; preds = %181
  %194 = add i64 %175, 1
  br label %174

195:                                              ; preds = %174
  %196 = call double @omp_get_wtime()
  %197 = call i32 @artsGetCurrentNode()
  %198 = alloca i64, i64 0, align 8
  %199 = call i64 @artsEdtCreate(ptr @__arts_edt_1, i32 %197, i32 0, ptr %198, i32 1)
  %200 = call i64 @artsInitializeAndStartEpoch(i64 %199, i32 0)
  %201 = call i32 @artsGetCurrentNode()
  %202 = mul i64 %142, %144
  %203 = add i64 %202, 2
  %204 = trunc i64 %203 to i32
  %205 = alloca i64, i64 2, align 8
  store i64 %144, ptr %205, align 8
  %206 = getelementptr i64, ptr %205, i32 1
  store i64 %142, ptr %206, align 8
  %207 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_2, i32 %201, i32 2, ptr %205, i32 %204, i64 %200)
  call void @artsDbAddDependence(i64 %2, i64 %207, i32 0)
  call void @artsDbAddDependence(i64 %5, i64 %207, i32 1)
  %208 = call ptr @malloc(i64 8)
  store i64 2, ptr %208, align 8
  br label %209

209:                                              ; preds = %228, %195
  %210 = phi i64 [ 0, %195 ], [ %229, %228 ]
  %211 = icmp slt i64 %210, %142
  br i1 %211, label %212, label %230

212:                                              ; preds = %209
  br label %213

213:                                              ; preds = %216, %212
  %214 = phi i64 [ 0, %212 ], [ %227, %216 ]
  %215 = icmp slt i64 %214, %144
  br i1 %215, label %216, label %228

216:                                              ; preds = %213
  %217 = load i64, ptr %208, align 8
  %218 = mul i64 %214, 1
  %219 = add i64 %218, 0
  %220 = mul i64 %144, 1
  %221 = mul i64 %210, %220
  %222 = add i64 %219, %221
  %223 = getelementptr i64, ptr %149, i64 %222
  %224 = load i64, ptr %223, align 8
  %225 = trunc i64 %217 to i32
  call void @artsDbAddDependence(i64 %224, i64 %207, i32 %225)
  %226 = add i64 %217, 1
  store i64 %226, ptr %208, align 8
  %227 = add i64 %214, 1
  br label %213

228:                                              ; preds = %213
  %229 = add i64 %210, 1
  br label %209

230:                                              ; preds = %209
  %231 = call ptr @malloc(i64 8)
  store i64 2, ptr %231, align 8
  br label %232

232:                                              ; preds = %250, %230
  %233 = phi i64 [ 0, %230 ], [ %251, %250 ]
  %234 = icmp slt i64 %233, %142
  br i1 %234, label %235, label %252

235:                                              ; preds = %232
  br label %236

236:                                              ; preds = %239, %235
  %237 = phi i64 [ 0, %235 ], [ %249, %239 ]
  %238 = icmp slt i64 %237, %144
  br i1 %238, label %239, label %250

239:                                              ; preds = %236
  %240 = mul i64 %237, 1
  %241 = add i64 %240, 0
  %242 = mul i64 %144, 1
  %243 = mul i64 %233, %242
  %244 = add i64 %241, %243
  %245 = getelementptr i64, ptr %149, i64 %244
  %246 = load i64, ptr %245, align 8
  call void @artsDbIncrementLatch(i64 %246)
  %247 = load i64, ptr %231, align 8
  %248 = add i64 %247, 1
  store i64 %248, ptr %231, align 8
  %249 = add i64 %237, 1
  br label %236

250:                                              ; preds = %236
  %251 = add i64 %233, 1
  br label %232

252:                                              ; preds = %232
  %253 = call i1 @artsWaitOnHandle(i64 %200)
  %254 = call double @omp_get_wtime()
  %255 = fsub double %254, %196
  %256 = call i32 (ptr, ...) @printf(ptr @str4, double %255)
  %257 = call i32 (ptr, ...) @printf(ptr @str5)
  br label %258

258:                                              ; preds = %312, %252
  %259 = phi i64 [ 1, %252 ], [ %313, %312 ]
  %260 = icmp slt i64 %259, %142
  br i1 %260, label %261, label %314

261:                                              ; preds = %258
  %262 = trunc i64 %259 to i32
  %263 = add i32 %262, -1
  %264 = sext i32 %263 to i64
  %265 = getelementptr i8, ptr %6, i64 %264
  br label %266

266:                                              ; preds = %310, %261
  %267 = phi i64 [ 1, %261 ], [ %311, %310 ]
  %268 = icmp slt i64 %267, %144
  br i1 %268, label %269, label %312

269:                                              ; preds = %266
  %270 = trunc i64 %267 to i32
  %271 = add i32 %270, -1
  %272 = sext i32 %271 to i64
  %273 = mul i64 %272, 1
  %274 = add i64 %273, 0
  %275 = mul i64 %144, 1
  %276 = mul i64 %264, %275
  %277 = add i64 %274, %276
  %278 = getelementptr i32, ptr %173, i64 %277
  %279 = load i32, ptr %278, align 4
  %280 = load i8, ptr %265, align 1
  %281 = getelementptr i8, ptr %3, i64 %272
  %282 = load i8, ptr %281, align 1
  %283 = icmp eq i8 %280, %282
  %284 = select i1 %283, i32 2, i32 -1
  %285 = add i32 %279, %284
  %286 = mul i64 %267, 1
  %287 = add i64 %286, 0
  %288 = add i64 %287, %276
  %289 = getelementptr i32, ptr %173, i64 %288
  %290 = load i32, ptr %289, align 4
  %291 = add i32 %290, -2
  %292 = mul i64 %259, %275
  %293 = add i64 %274, %292
  %294 = getelementptr i32, ptr %173, i64 %293
  %295 = load i32, ptr %294, align 4
  %296 = add i32 %295, -2
  %297 = icmp sgt i32 %285, %291
  br i1 %297, label %298, label %349

298:                                              ; preds = %349, %269
  %299 = phi i32 [ %350, %349 ], [ %285, %269 ]
  %300 = phi i32 [ %351, %349 ], [ %285, %269 ]
  %301 = icmp sgt i32 %299, %296
  %302 = select i1 %301, i32 %300, i32 %296
  br label %303

303:                                              ; preds = %298
  %304 = phi i32 [ %302, %298 ]
  br label %305

305:                                              ; preds = %303
  %306 = add i64 %287, %292
  %307 = getelementptr i32, ptr %173, i64 %306
  store i32 %304, ptr %307, align 4
  %308 = icmp slt i32 %304, 0
  br i1 %308, label %309, label %310

309:                                              ; preds = %305
  store i32 0, ptr %307, align 4
  br label %310

310:                                              ; preds = %309, %305
  %311 = add i64 %267, 1
  br label %266

312:                                              ; preds = %266
  %313 = add i64 %259, 1
  br label %258

314:                                              ; preds = %258
  br label %315

315:                                              ; preds = %340, %314
  %316 = phi i64 [ 0, %314 ], [ %341, %340 ]
  %317 = phi i32 [ 1, %314 ], [ %325, %340 ]
  %318 = icmp slt i64 %316, %142
  br i1 %318, label %319, label %342

319:                                              ; preds = %315
  %320 = mul i64 %144, 1
  %321 = mul i64 %316, %320
  %322 = getelementptr ptr, ptr %150, i64 %321
  br label %323

323:                                              ; preds = %327, %319
  %324 = phi i64 [ 0, %319 ], [ %339, %327 ]
  %325 = phi i32 [ %317, %319 ], [ %338, %327 ]
  %326 = icmp slt i64 %324, %144
  br i1 %326, label %327, label %340

327:                                              ; preds = %323
  %328 = mul i64 %324, 8
  %329 = getelementptr i8, ptr %322, i64 %328
  %330 = load ptr, ptr %329, align 8
  %331 = load i32, ptr %330, align 4
  %332 = mul i64 %324, 1
  %333 = add i64 %332, 0
  %334 = add i64 %333, %321
  %335 = getelementptr i32, ptr %173, i64 %334
  %336 = load i32, ptr %335, align 4
  %337 = icmp ne i32 %331, %336
  %338 = select i1 %337, i32 0, i32 %325
  %339 = add i64 %324, 1
  br label %323

340:                                              ; preds = %323
  %341 = add i64 %316, 1
  br label %315

342:                                              ; preds = %315
  %343 = icmp ne i32 %317, 0
  br i1 %343, label %344, label %346

344:                                              ; preds = %342
  %345 = call i32 (ptr, ...) @printf(ptr @str6)
  br label %348

346:                                              ; preds = %342
  %347 = call i32 (ptr, ...) @printf(ptr @str7)
  br label %348

348:                                              ; preds = %344, %346
  ret i32 0

349:                                              ; preds = %269
  %350 = phi i32 [ %291, %269 ]
  %351 = phi i32 [ %291, %269 ]
  br label %298
}

declare i64 @strlen(ptr)

declare double @omp_get_wtime()

define void @__arts_edt_1(i32 %0, ptr %1, i32 %2, ptr %3) {
  ret void
}

define void @__arts_edt_2(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = getelementptr i64, ptr %1, i32 1
  %8 = load i64, ptr %7, align 8
  %9 = alloca i64, i64 1, align 8
  store i64 0, ptr %9, align 8
  %10 = load i64, ptr %9, align 8
  %11 = mul i64 %10, 24
  %12 = trunc i64 %11 to i32
  %13 = getelementptr i8, ptr %3, i32 %12
  %14 = getelementptr { i64, i32, ptr }, ptr %13, i32 0, i32 2
  %15 = load ptr, ptr %14, align 8
  %16 = add i64 %10, 1
  store i64 %16, ptr %9, align 8
  %17 = load i64, ptr %9, align 8
  %18 = mul i64 %17, 24
  %19 = trunc i64 %18 to i32
  %20 = getelementptr i8, ptr %3, i32 %19
  %21 = getelementptr { i64, i32, ptr }, ptr %20, i32 0, i32 2
  %22 = load ptr, ptr %21, align 8
  %23 = add i64 %17, 1
  store i64 %23, ptr %9, align 8
  %24 = mul i64 %8, 1
  %25 = mul i64 %24, %6
  %26 = mul i64 %25, 8
  %27 = alloca i8, i64 %26, align 1
  br label %28

28:                                               ; preds = %50, %4
  %29 = phi i64 [ 0, %4 ], [ %51, %50 ]
  %30 = icmp slt i64 %29, %8
  br i1 %30, label %31, label %52

31:                                               ; preds = %28
  br label %32

32:                                               ; preds = %35, %31
  %33 = phi i64 [ 0, %31 ], [ %49, %35 ]
  %34 = icmp slt i64 %33, %6
  br i1 %34, label %35, label %50

35:                                               ; preds = %32
  %36 = load i64, ptr %9, align 8
  %37 = mul i64 %36, 24
  %38 = trunc i64 %37 to i32
  %39 = getelementptr i8, ptr %3, i32 %38
  %40 = getelementptr { i64, i32, ptr }, ptr %39, i32 0, i32 0
  %41 = load i64, ptr %40, align 8
  %42 = mul i64 %33, 1
  %43 = add i64 %42, 0
  %44 = mul i64 %6, 1
  %45 = mul i64 %29, %44
  %46 = add i64 %43, %45
  %47 = getelementptr i64, ptr %27, i64 %46
  store i64 %41, ptr %47, align 8
  %48 = add i64 %36, 1
  store i64 %48, ptr %9, align 8
  %49 = add i64 %33, 1
  br label %32

50:                                               ; preds = %32
  %51 = add i64 %29, 1
  br label %28

52:                                               ; preds = %28
  %53 = call i32 (ptr, ...) @printf(ptr @str2, ptr %22)
  %54 = call i32 (ptr, ...) @printf(ptr @str3, ptr %15)
  %55 = alloca i64, i64 1, align 8
  %56 = alloca i64, i64 1, align 8
  br label %57

57:                                               ; preds = %119, %52
  %58 = phi i64 [ 1, %52 ], [ %120, %119 ]
  %59 = icmp slt i64 %58, %8
  br i1 %59, label %60, label %121

60:                                               ; preds = %57
  %61 = trunc i64 %58 to i32
  %62 = add i32 %61, -1
  %63 = sext i32 %62 to i64
  %64 = getelementptr i8, ptr %22, i64 %63
  br label %65

65:                                               ; preds = %68, %60
  %66 = phi i64 [ 1, %60 ], [ %118, %68 ]
  %67 = icmp slt i64 %66, %6
  br i1 %67, label %68, label %119

68:                                               ; preds = %65
  %69 = trunc i64 %66 to i32
  %70 = add i32 %69, -1
  %71 = sext i32 %70 to i64
  %72 = load i8, ptr %64, align 1
  %73 = getelementptr i8, ptr %15, i64 %71
  %74 = load i8, ptr %73, align 1
  %75 = call i64 @artsGetCurrentEpochGuid()
  %76 = call i32 @artsGetCurrentNode()
  store i64 0, ptr %55, align 8
  %77 = load i64, ptr %55, align 8
  %78 = add i64 %77, 1
  store i64 %78, ptr %55, align 8
  %79 = load i64, ptr %55, align 8
  %80 = add i64 %79, 1
  store i64 %80, ptr %55, align 8
  %81 = load i64, ptr %55, align 8
  %82 = add i64 %81, 1
  store i64 %82, ptr %55, align 8
  %83 = load i64, ptr %55, align 8
  %84 = add i64 %83, 1
  store i64 %84, ptr %55, align 8
  %85 = load i64, ptr %55, align 8
  %86 = trunc i64 %85 to i32
  store i64 2, ptr %56, align 8
  %87 = load i64, ptr %56, align 8
  %88 = trunc i64 %87 to i32
  %89 = alloca i64, i64 %87, align 8
  %90 = sext i8 %72 to i64
  store i64 %90, ptr %89, align 8
  %91 = sext i8 %74 to i64
  %92 = getelementptr i64, ptr %89, i32 1
  store i64 %91, ptr %92, align 8
  %93 = call i64 @artsEdtCreateWithEpoch(ptr @__arts_edt_3, i32 %76, i32 %88, ptr %89, i32 %86, i64 %75)
  %94 = mul i64 %71, 1
  %95 = add i64 %94, 0
  %96 = mul i64 %6, 1
  %97 = mul i64 %63, %96
  %98 = add i64 %95, %97
  %99 = getelementptr i64, ptr %27, i64 %98
  %100 = load i64, ptr %99, align 8
  %101 = trunc i64 %77 to i32
  call void @artsDbAddDependence(i64 %100, i64 %93, i32 %101)
  %102 = mul i64 %66, 1
  %103 = add i64 %102, 0
  %104 = add i64 %103, %97
  %105 = getelementptr i64, ptr %27, i64 %104
  %106 = load i64, ptr %105, align 8
  %107 = trunc i64 %79 to i32
  call void @artsDbAddDependence(i64 %106, i64 %93, i32 %107)
  %108 = mul i64 %58, %96
  %109 = add i64 %95, %108
  %110 = getelementptr i64, ptr %27, i64 %109
  %111 = load i64, ptr %110, align 8
  %112 = trunc i64 %81 to i32
  call void @artsDbAddDependence(i64 %111, i64 %93, i32 %112)
  %113 = add i64 %103, %108
  %114 = getelementptr i64, ptr %27, i64 %113
  %115 = load i64, ptr %114, align 8
  %116 = trunc i64 %83 to i32
  call void @artsDbAddDependence(i64 %115, i64 %93, i32 %116)
  %117 = load i64, ptr %114, align 8
  call void @artsDbIncrementLatch(i64 %117)
  %118 = add i64 %66, 1
  br label %65

119:                                              ; preds = %65
  %120 = add i64 %58, 1
  br label %57

121:                                              ; preds = %57
  ret void
}

define void @__arts_edt_3(i32 %0, ptr %1, i32 %2, ptr %3) {
  %5 = call i64 @artsGetCurrentGuid()
  %6 = load i64, ptr %1, align 8
  %7 = trunc i64 %6 to i8
  %8 = getelementptr i64, ptr %1, i32 1
  %9 = load i64, ptr %8, align 8
  %10 = trunc i64 %9 to i8
  %11 = getelementptr { i64, i32, ptr }, ptr %3, i32 0, i32 2
  %12 = load ptr, ptr %11, align 8
  %13 = getelementptr i8, ptr %3, i32 24
  %14 = getelementptr { i64, i32, ptr }, ptr %13, i32 0, i32 2
  %15 = load ptr, ptr %14, align 8
  %16 = getelementptr i8, ptr %3, i32 48
  %17 = getelementptr { i64, i32, ptr }, ptr %16, i32 0, i32 2
  %18 = load ptr, ptr %17, align 8
  %19 = getelementptr i8, ptr %3, i32 72
  %20 = getelementptr { i64, i32, ptr }, ptr %19, i32 0, i32 2
  %21 = load ptr, ptr %20, align 8
  %22 = load i32, ptr %12, align 4
  %23 = icmp eq i8 %7, %10
  %24 = select i1 %23, i32 2, i32 -1
  %25 = add i32 %22, %24
  %26 = load i32, ptr %15, align 4
  %27 = add i32 %26, -2
  %28 = load i32, ptr %18, align 4
  %29 = add i32 %28, -2
  %30 = icmp sgt i32 %25, %27
  br i1 %30, label %31, label %42

31:                                               ; preds = %42, %4
  %32 = phi i32 [ %43, %42 ], [ %25, %4 ]
  %33 = phi i32 [ %44, %42 ], [ %25, %4 ]
  %34 = icmp sgt i32 %32, %29
  %35 = select i1 %34, i32 %33, i32 %29
  br label %36

36:                                               ; preds = %31
  %37 = phi i32 [ %35, %31 ]
  br label %38

38:                                               ; preds = %36
  store i32 %37, ptr %21, align 4
  %39 = icmp slt i32 %37, 0
  br i1 %39, label %40, label %41

40:                                               ; preds = %38
  store i32 0, ptr %21, align 4
  br label %41

41:                                               ; preds = %40, %38
  ret void

42:                                               ; preds = %4
  %43 = phi i32 [ %27, %4 ]
  %44 = phi i32 [ %27, %4 ]
  br label %31
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

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}

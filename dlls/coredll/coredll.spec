2 stdcall InitializeCriticalSection(ptr) kernel32.InitializeCriticalSection
3 stdcall DeleteCriticalSection(ptr) kernel32.DeleteCriticalSection
4 stdcall EnterCriticalSection(ptr) kernel32.EnterCriticalSection
5 stdcall LeaveCriticalSection(ptr) kernel32.LeaveCriticalSection
6 stdcall ExitThread(long) kernel32.ExitThread
7 stub -noname PSLNotify
8 stub -noname InitLocale
9 stub -noname InterlockedTestExchange
10 stdcall -arch=i386 InterlockedIncrement(ptr) kernel32.InterlockedIncrement
11 stdcall -arch=i386 InterlockedDecrement(ptr) kernel32.InterlockedDecrement
12 stdcall -arch=i386 InterlockedExchange(ptr long) kernel32.InterlockedExchange
13 stub -noname ThreadBaseFunc
14 stub -noname MainThreadBaseFunc
15 stdcall TlsGetValue(long) kernel32.TlsGetValue
16 stdcall TlsSetValue(long ptr) kernel32.TlsSetValue
17 stub -noname GetVersionEx
18 stdcall CompareFileTime(ptr ptr) kernel32.CompareFileTime
19 stdcall SystemTimeToFileTime(ptr ptr) kernel32.SystemTimeToFileTime
20 stdcall FileTimeToSystemTime(ptr ptr) kernel32.FileTimeToSystemTime
21 stdcall FileTimeToLocalFileTime(ptr ptr) kernel32.FileTimeToLocalFileTime
22 stdcall LocalFileTimeToFileTime(ptr ptr) kernel32.LocalFileTimeToFileTime
23 stdcall GetLocalTime(ptr) kernel32.GetLocalTime
24 stdcall SetLocalTime(ptr) kernel32.SetLocalTime
25 stdcall GetSystemTime(ptr) kernel32.GetSystemTime
26 stdcall SetSystemTime(ptr) kernel32.SetSystemTime
27 stdcall GetTimeZoneInformation(ptr) kernel32.GetTimeZoneInformation
28 stdcall SetTimeZoneInformation(ptr) kernel32.SetTimeZoneInformation
29 stub -noname GetCurrentFT
30 stub -noname IsAPIReady
31 cdecl memchr(ptr long long) msvcrt.memchr
32 stub -noname GetAPIAddress
33 stdcall LocalAlloc(long long) kernel32.LocalAlloc
34 stdcall LocalReAlloc(long long long) kernel32.LocalReAlloc
35 stdcall LocalSize(long) kernel32.LocalSize
36 stdcall LocalFree(long) kernel32.LocalFree
37 stub -noname RemoteLocalAlloc
38 stub -noname RemoteLocalReAlloc
39 stub -noname RemoteLocalSize
40 stub -noname RemoteLocalFree
41 stub -noname LocalAllocInProcess
42 stub -noname LocalFreeInProcess
43 stub -noname LocalSizeInProcess
44 stdcall HeapCreate(long long long) kernel32.HeapCreate
45 stdcall HeapDestroy(long) kernel32.HeapDestroy
46 stdcall HeapAlloc(long long long) kernel32.HeapAlloc
47 stdcall HeapReAlloc(long long ptr long) kernel32.HeapReAlloc
48 stdcall HeapSize(long long ptr) kernel32.HeapSize
49 stdcall HeapFree(long long long) kernel32.HeapFree
50 stdcall GetProcessHeap() kernel32.GetProcessHeap
51 stdcall HeapValidate(long long ptr) kernel32.HeapValidate
52 stub -noname GetHeapSnapshot
53 stub -noname CeModuleJit
54 stub -noname CompactAllHeaps
55 stub LocalAllocTrace
56 varargs wsprintfW(wstr wstr) user32.wsprintfW
57 stdcall wvsprintfW(ptr wstr ptr) user32.wvsprintfW
58 cdecl wcscat(wstr wstr) msvcrt.wcscat
59 cdecl wcschr(wstr long) msvcrt.wcschr
60 cdecl wcscmp(wstr wstr) msvcrt.wcscmp
61 cdecl wcscpy(ptr wstr) msvcrt.wcscpy
62 cdecl wcscspn(wstr wstr) msvcrt.wcscspn
63 cdecl wcslen(wstr) msvcrt.wcslen
64 cdecl wcsncat(wstr wstr long) msvcrt.wcsncat
65 cdecl wcsncmp(wstr wstr long) msvcrt.wcsncmp
66 cdecl wcsncpy(ptr wstr long) msvcrt.wcsncpy
67 cdecl _wcsnset(wstr long long) msvcrt._wcsnset
68 cdecl wcspbrk(wstr wstr) msvcrt.wcspbrk
69 cdecl wcsrchr(wstr long) msvcrt.wcsrchr
70 cdecl _wcsrev(wstr) msvcrt._wcsrev
71 cdecl _wcsset(wstr long) msvcrt._wcsset
72 cdecl wcsspn(wstr wstr) msvcrt.wcsspn
73 cdecl wcsstr(wstr wstr) msvcrt.wcsstr
74 cdecl _wcsdup(wstr) msvcrt._wcsdup
75 cdecl wcstombs(ptr ptr long) msvcrt.wcstombs
76 cdecl mbstowcs(ptr str long) msvcrt.mbstowcs
77 cdecl wcstok(wstr wstr) msvcrt.wcstok
78 cdecl _wtol(wstr) msvcrt._wtol
79 stub -noname _wtoll
80 stub -noname Random
82 stub -noname ProfileStart
83 stub -noname ProfileStop
87 stub -noname __C_specific_handler(ptr long ptr ptr)
88 stdcall GlobalMemoryStatus(ptr) kernel32.GlobalMemoryStatus
89 stdcall SystemParametersInfoW(long long ptr long) user32.SystemParametersInfoW
90 stdcall CreateDIBSection(long ptr long ptr long long) gdi32.CreateDIBSection
91 stdcall EqualRgn(long long) gdi32.EqualRgn
92 stdcall CreateAcceleratorTableW(ptr long) user32.CreateAcceleratorTableW
93 stdcall DestroyAcceleratorTable(long) user32.DestroyAcceleratorTable
94 stdcall LoadAcceleratorsW(long wstr) user32.LoadAcceleratorsW
95 stdcall RegisterClassW(ptr) user32.RegisterClassW
96 stdcall CopyRect(ptr ptr) user32.CopyRect
97 stdcall EqualRect(ptr ptr) user32.EqualRect
98 stdcall InflateRect(ptr long long) user32.InflateRect
99 stdcall IntersectRect(ptr ptr ptr) user32.IntersectRect
100 stdcall IsRectEmpty(ptr) user32.IsRectEmpty
101 stdcall OffsetRect(ptr long long) user32.OffsetRect
102 stdcall PtInRect(ptr int64) user32.PtInRect
103 stdcall SetRect(ptr long long long long) user32.SetRect
104 stdcall SetRectEmpty(ptr) user32.SetRectEmpty
105 stdcall SubtractRect(ptr ptr ptr) user32.SubtractRect
106 stdcall UnionRect(ptr ptr ptr) user32.UnionRect
107 stdcall ClearCommBreak(long) kernel32.ClearCommBreak
108 stdcall ClearCommError(long ptr ptr) kernel32.ClearCommError
109 stdcall EscapeCommFunction(long long) kernel32.EscapeCommFunction
110 stdcall GetCommMask(long ptr) kernel32.GetCommMask
111 stdcall GetCommModemStatus(long ptr) kernel32.GetCommModemStatus
112 stdcall GetCommProperties(long ptr) kernel32.GetCommProperties
113 stdcall GetCommState(long ptr) kernel32.GetCommState
114 stdcall GetCommTimeouts(long ptr) kernel32.GetCommTimeouts
115 stdcall PurgeComm(long long) kernel32.PurgeComm
116 stdcall SetCommBreak(long) kernel32.SetCommBreak
117 stdcall SetCommMask(long ptr) kernel32.SetCommMask
118 stdcall SetCommState(long ptr) kernel32.SetCommState
119 stdcall SetCommTimeouts(long ptr) kernel32.SetCommTimeouts
120 stdcall SetupComm(long long long) kernel32.SetupComm
121 stdcall TransmitCommChar(long long) kernel32.TransmitCommChar
122 stdcall WaitCommEvent(long ptr ptr) kernel32.WaitCommEvent
123 stub -noname EnumPnpIds
124 stub -noname EnumDevices
125 stub -noname GetDeviceKeys
126 stdcall CryptAcquireContextW(ptr wstr wstr long long) unicows.CryptAcquireContextW
127 stub -noname CryptReleaseContext
128 stub -noname CryptGenKey
129 stub -noname CryptDeriveKey
130 stub -noname CryptDestroyKey
131 stub -noname CryptSetKeyParam
132 stub -noname CryptGetKeyParam
133 stub -noname CryptExportKey
134 stub -noname CryptImportKey
135 stub -noname CryptEncrypt
136 stub -noname CryptDecrypt
137 stub -noname CryptCreateHash
138 stub -noname CryptHashSessionKey
139 stub -noname CryptHashData
140 stub -noname CryptDestroyHash
141 stdcall CryptSignHashW(long long ptr long ptr ptr) unicows.CryptSignHashW
142 stdcall CryptVerifySignatureW(long ptr long long ptr long) unicows.CryptVerifySignatureW
143 stub -noname CryptGenRandom
144 stub -noname CryptGetUserKey
145 stdcall CryptSetProviderW(wstr long) unicows.CryptSetProviderW
146 stub -noname CryptGetHashParam
147 stub -noname CryptSetHashParam
148 stub -noname CryptGetProvParam
149 stub -noname CryptSetProvParam
150 stdcall CryptSetProviderExW(wstr long ptr long) unicows.CryptSetProviderExW
151 stdcall CryptGetDefaultProviderW(long ptr long ptr ptr) unicows.CryptGetDefaultProviderW
152 stdcall CryptEnumProviderTypesW(long ptr long ptr ptr ptr) unicows.CryptEnumProviderTypesW
153 stdcall CryptEnumProvidersW(long ptr long ptr ptr ptr) unicows.CryptEnumProvidersW
154 stub -noname CryptContextAddRef
155 stub -noname CryptDuplicateKey
156 stub -noname CryptDuplicateHash
157 stub -noname AttachDebugger
158 stub -noname SetInterruptEvent
159 stub -noname IsExiting
160 stdcall CreateDirectoryW(wstr ptr) kernel32.CreateDirectoryW
161 stdcall RemoveDirectoryW(wstr) kernel32.RemoveDirectoryW
162 stdcall GetTempPathW(long ptr) kernel32.GetTempPathW
163 stdcall MoveFileW(wstr wstr) kernel32.MoveFileW
164 stdcall CopyFileW(wstr wstr long) kernel32.CopyFileW
165 stdcall DeleteFileW(wstr) kernel32.DeleteFileW
166 stdcall GetFileAttributesW(wstr) kernel32.GetFileAttributesW
167 stdcall FindFirstFileW(wstr ptr) kernel32.FindFirstFileW
168 stdcall CreateFileW(wstr long long ptr long long long) kernel32.CreateFileW
169 stdcall SetFileAttributesW(wstr long) kernel32.SetFileAttributesW
170 stdcall ReadFile(long ptr long ptr ptr) kernel32.ReadFile
171 stdcall WriteFile(long ptr long ptr ptr) kernel32.WriteFile
172 stdcall GetFileSize(long ptr) kernel32.GetFileSize
173 stdcall SetFilePointer(long long ptr long) kernel32.SetFilePointer
174 stdcall GetFileInformationByHandle(long ptr) kernel32.GetFileInformationByHandle
175 stdcall FlushFileBuffers(long) kernel32.FlushFileBuffers
176 stdcall GetFileTime(long ptr ptr ptr) kernel32.GetFileTime
177 stdcall SetFileTime(long ptr ptr ptr) kernel32.SetFileTime
178 stdcall SetEndOfFile(long) kernel32.SetEndOfFile
179 stdcall DeviceIoControl(long long ptr long ptr long ptr ptr) kernel32.DeviceIoControl
180 stdcall FindClose(long) kernel32.FindClose
181 stdcall FindNextFileW(long ptr) kernel32.FindNextFileW
182 stub -noname CheckPassword
183 stub -noname DeleteAndRenameFile
184 stdcall GetDiskFreeSpaceExW(wstr ptr ptr ptr) kernel32.GetDiskFreeSpaceExW
185 stdcall IsValidCodePage(long) kernel32.IsValidCodePage
186 stdcall GetACP() kernel32.GetACP
187 stdcall GetOEMCP() kernel32.GetOEMCP
188 stdcall GetCPInfo(long ptr) kernel32.GetCPInfo
189 stub -noname SetACP
190 stub -noname SetOEMCP
191 stdcall IsDBCSLeadByte(long) kernel32.IsDBCSLeadByte
192 stdcall IsDBCSLeadByteEx(long long) kernel32.IsDBCSLeadByteEx
193 cdecl iswctype(long long) msvcrt.iswctype
194 cdecl towlower(long) msvcrt.towlower
195 cdecl towupper(long) msvcrt.towupper
196 stdcall MultiByteToWideChar(long long str long ptr long) kernel32.MultiByteToWideChar
197 stdcall WideCharToMultiByte(long long wstr long ptr long ptr ptr) kernel32.WideCharToMultiByte
198 stdcall CompareStringW(long long wstr long wstr long) kernel32.CompareStringW
199 stdcall LCMapStringW(long long wstr long ptr long) kernel32.LCMapStringW
200 stdcall GetLocaleInfoW(long long ptr long) kernel32.GetLocaleInfoW
201 stdcall SetLocaleInfoW(long long wstr) kernel32.SetLocaleInfoW
202 stdcall GetTimeFormatW(long long ptr wstr ptr long) kernel32.GetTimeFormatW
203 stdcall GetDateFormatW(long long ptr wstr ptr long) kernel32.GetDateFormatW
204 stdcall GetNumberFormatW(long long wstr ptr ptr long) kernel32.GetNumberFormatW
205 stdcall GetCurrencyFormatW(long long str ptr str long) kernel32.GetCurrencyFormatW
206 stdcall EnumCalendarInfoW(ptr long long long) kernel32.EnumCalendarInfoW
207 stdcall EnumTimeFormatsW(ptr long long) kernel32.EnumTimeFormatsW
208 stdcall EnumDateFormatsW(ptr long long) kernel32.EnumDateFormatsW
209 stdcall IsValidLocale(long long) kernel32.IsValidLocale
210 stdcall ConvertDefaultLocale(long) kernel32.ConvertDefaultLocale
211 stdcall GetSystemDefaultLangID() kernel32.GetSystemDefaultLangID
212 stdcall GetUserDefaultLangID() kernel32.GetUserDefaultLangID
213 stdcall GetSystemDefaultLCID() kernel32.GetSystemDefaultLCID
214 stub -noname SetSystemDefaultLCID
215 stdcall GetUserDefaultLCID() kernel32.GetUserDefaultLCID
216 stdcall GetStringTypeW(long wstr long ptr) kernel32.GetStringTypeW
217 stdcall GetStringTypeExW(long long wstr long ptr) kernel32.GetStringTypeExW
218 stdcall FoldStringW(long wstr long ptr long) kernel32.FoldStringW
219 stdcall EnumSystemLocalesW(ptr long) kernel32.EnumSystemLocalesW
220 stdcall EnumSystemCodePagesW(ptr long) kernel32.EnumSystemCodePagesW
221 stdcall CharLowerW(wstr) user32.CharLowerW
222 stdcall CharLowerBuffW(wstr long) user32.CharLowerBuffW
223 stdcall CharUpperBuffW(wstr long) user32.CharUpperBuffW
224 stdcall CharUpperW(wstr) user32.CharUpperW
225 stdcall CharPrevW(wstr wstr) user32.CharPrevW
226 stdcall CharNextW(wstr) user32.CharNextW
227 stdcall lstrcmpW(wstr wstr) kernel32.lstrcmpW
228 stdcall lstrcmpiW(wstr wstr) kernel32.lstrcmpiW
229 cdecl _wcsnicmp(wstr wstr long) msvcrt._wcsnicmp
230 cdecl _wcsicmp(wstr wstr) msvcrt._wcsicmp
231 cdecl _wcslwr(wstr) msvcrt._wcslwr
232 cdecl _wcsupr(wstr) msvcrt._wcsupr
233 stub -noname DBCanonicalize
234 stdcall FormatMessageW(long ptr long long ptr long ptr) kernel32.FormatMessageW
235 stub -noname RegisterDevice
236 stub -noname DeregisterDevice
237 stub -noname LoadFSD
238 stub -noname SetPassword
239 stub -noname GetPasswordActive
240 stub -noname SetPasswordActive
241 stub -noname FileSystemPowerFunction
242 stub -noname CloseAllFileHandles
243 stub -noname ReadFileWithSeek
245 stub -noname CreateDeviceHandle
246 stdcall CreateWindowExW(long wstr wstr long long long long long long long long ptr) user32.CreateWindowExW
247 stdcall SetWindowPos(long long long long long long long) user32.SetWindowPos
248 stdcall GetWindowRect(long ptr) user32.GetWindowRect
249 stdcall GetClientRect(long long) user32.GetClientRect
250 stdcall InvalidateRect(long ptr long) user32.InvalidateRect
251 stdcall GetWindow(long long) user32.GetWindow
252 stdcall WindowFromPoint(int64) user32.WindowFromPoint
253 stdcall ChildWindowFromPoint(long int64) user32.ChildWindowFromPoint
254 stdcall ClientToScreen(long ptr) user32.ClientToScreen
255 stdcall ScreenToClient(long ptr) user32.ScreenToClient
256 stdcall SetWindowTextW(long wstr) user32.SetWindowTextW
257 stdcall GetWindowTextW(long ptr long) user32.GetWindowTextW
258 stdcall SetWindowLongW(long long long) user32.SetWindowLongW
259 stdcall GetWindowLongW(long long) user32.GetWindowLongW
260 stdcall BeginPaint(long ptr) user32.BeginPaint
261 stdcall EndPaint(long ptr) user32.EndPaint
262 stdcall GetDC(long) user32.GetDC
263 stdcall ReleaseDC(long long) user32.ReleaseDC
264 stdcall -register DefWindowProcW(long long long long) user32.DefWindowProcW
265 stdcall DestroyWindow(long) user32.DestroyWindow
266 stdcall ShowWindow(long long) user32.ShowWindow
267 stdcall UpdateWindow(long) user32.UpdateWindow
268 stdcall SetParent(long long) user32.SetParent
269 stdcall GetParent(long) user32.GetParent
270 stdcall GetWindowDC(long) user32.GetWindowDC
271 stdcall IsWindow(long) user32.IsWindow
272 stdcall MoveWindow(long long long long long long) user32.MoveWindow
273 stdcall GetUpdateRgn(long long long) user32.GetUpdateRgn
274 stdcall GetUpdateRect(long ptr long) user32.GetUpdateRect
275 stdcall BringWindowToTop(long) user32.BringWindowToTop
276 stdcall GetWindowTextLengthW(long) user32.GetWindowTextLengthW
277 stdcall IsChild(long long) user32.IsChild
278 stdcall ValidateRect(long ptr) user32.ValidateRect
279 stdcall SetScrollInfo(long long ptr long) user32.SetScrollInfo
280 stdcall SetScrollPos(long long long long) user32.SetScrollPos
281 stdcall SetScrollRange(long long long long long) user32.SetScrollRange
282 stdcall GetScrollInfo(long long ptr) user32.GetScrollInfo
283 stdcall GetClassNameW(long ptr long) user32.GetClassNameW
284 stdcall MapWindowPoints(long long ptr long) user32.MapWindowPoints
285 stdcall CallWindowProcW(ptr long long long long) user32.CallWindowProcW
286 stdcall FindWindowW(wstr wstr) user32.FindWindowW
287 stdcall EnableWindow(long long) user32.EnableWindow
288 stdcall IsWindowEnabled(long) user32.IsWindowEnabled
289 stdcall ScrollWindowEx(long long long ptr ptr long ptr long) user32.ScrollWindowEx
290 stdcall PostThreadMessageW(long long long long) user32.PostThreadMessageW
291 stdcall EnumWindows(ptr long) user32.EnumWindows
292 stdcall GetWindowThreadProcessId(long ptr) user32.GetWindowThreadProcessId
293 stub -noname RegisterSIPanel
294 stub -noname RectangleAnimation
295 stub -noname SHGetSpecialFolderPath
296 stub -noname GwesPowerOffSystem
297 stub -noname BatteryDrvrGetLevels
298 stub -noname BatteryDrvrSupportsChangeNotification
299 stub -noname SetAssociatedMenu
300 stub -noname GetAssociatedMenu
301 stub -noname PegOidGetInfo
302 stub -noname PegFindFirstDatabase
303 stub -noname PegFindNextDatabase
304 stub -noname PegCreateDatabase
305 stub -noname PegSetDatabaseInfo
306 stub -noname PegOpenDatabase
307 stub -noname PegDeleteDatabase
308 stub -noname PegSeekDatabase
309 stub -noname PegDeleteRecord
310 stub -noname PegReadRecordProps
311 stub -noname PegWriteRecordProps
312 stub -noname CeOidGetInfo
313 stub -noname CeFindFirstDatabase
314 stub -noname CeFindNextDatabase
315 stub -noname CeCreateDatabase
316 stub -noname CeSetDatabaseInfo
317 stub -noname CeOpenDatabase
318 stub -noname CeDeleteDatabase
319 stub -noname CeSeekDatabase
320 stub -noname CeDeleteRecord
321 stub -noname CeReadRecordProps
322 stub -noname CeWriteRecordProps
323 stub -noname GetStoreInformation
324 stub -noname CeGetReplChangeMask
325 stub -noname CeSetReplChangeMask
326 stub -noname CeGetReplChangeBitsEx
327 stub -noname CeSetReplChangeBitsEx
328 stub -noname CeClearReplChangeBitsEx
329 stub -noname CeGetReplOtherBitsEx
330 stub -noname CeSetReplOtherBitsEx
331 stub -noname CeRegisterFileSystemNotification
332 stub -noname CeRegisterReplNotification
335 stub -noname DeregisterAFS
336 stub -noname GetSystemMemoryDivision
337 stub -noname SetSystemMemoryDivision
338 stub -noname RegisterAFSName
339 stub -noname DeregisterAFSName
340 stub -noname CeChangeDatabaseLCID
341 stub -noname DumpFileSystemHeap
342 stub -noname RasDial
343 stub -noname RasHangup
344 stub -noname RasHangUp
345 stub -noname RasEnumEntries
346 stub -noname RasGetEntryDialParams
347 stub -noname RasSetEntryDialParams
348 stub -noname RasGetEntryProperties
349 stub -noname RasSetEntryProperties
350 stub -noname RasValidateEntryName
351 stub -noname RasDeleteEntry
352 stub -noname RasRenameEntry
353 stub -noname RasEnumConnections
354 stub -noname RasGetConnectStatus
355 stub -noname RasGetEntryDevConfig
356 stub -noname RasSetEntryDevConfig
357 stub -noname RasIOControl
358 stub -noname lineClose
359 stub -noname lineDeallocateCall
360 stub -noname lineDrop
361 stub -noname lineGetDevCaps
362 stub -noname lineGetDevConfig
363 stub -noname lineGetTranslateCaps
364 stub -noname lineInitialize
365 stub -noname lineMakeCall
366 stub -noname lineNegotiateAPIVersion
367 stub -noname lineOpen
368 stub -noname lineSetDevConfig
369 stub -noname lineSetStatusMessages
370 stub -noname lineShutdown
371 stub -noname lineTranslateAddress
372 stub -noname lineGetID
373 stub -noname lineTranslateDialog
374 stub -noname lineConfigDialogEdit
375 stub -noname lineAddProvider
376 stub -noname AudioUpdateFromRegistry
377 stdcall sndPlaySoundW(ptr long) unicows.sndPlaySoundW
378 stdcall PlaySoundW(ptr long long) unicows.PlaySoundW
379 stub -noname waveOutGetNumDevs
380 stub -noname waveOutGetDevCaps
381 stub -noname waveOutGetVolume
382 stub -noname waveOutSetVolume
383 stub -noname waveOutGetErrorText
384 stub -noname waveOutClose
385 stub -noname waveOutPrepareHeader
386 stub -noname waveOutUnprepareHeader
387 stub -noname waveOutWrite
388 stub -noname waveOutPause
389 stub -noname waveOutRestart
390 stub -noname waveOutReset
391 stub -noname waveOutBreakLoop
392 stub -noname waveOutGetPosition
393 stub -noname waveOutGetPitch
394 stub -noname waveOutSetPitch
395 stub -noname waveOutGetPlaybackRate
396 stub -noname waveOutSetPlaybackRate
397 stub -noname waveOutGetID
398 stub -noname waveOutMessage
399 stub -noname waveOutOpen
400 stub -noname waveInGetNumDevs
401 stub -noname waveInGetDevCaps
402 stub -noname waveInGetErrorText
403 stub -noname waveInClose
404 stub -noname waveInPrepareHeader
405 stub -noname waveInUnprepareHeader
406 stub -noname waveInAddBuffer
407 stub -noname waveInStart
408 stub -noname waveInStop
409 stub -noname waveInReset
410 stub -noname waveInGetPosition
411 stub -noname waveInGetID
412 stub -noname waveInMessage
413 stub -noname waveInOpen
414 stub -noname acmDriverAdd
415 stub -noname acmDriverClose
416 stub -noname acmDriverDetails
417 stub -noname acmDriverEnum
418 stub -noname acmDriverID
419 stub -noname acmDriverMessage
420 stub -noname acmDriverOpen
421 stub -noname acmDriverPriority
422 stub -noname acmDriverRemove
423 stub -noname acmFilterChoose
424 stub -noname acmFilterDetails
425 stub -noname acmFilterEnum
426 stub -noname acmFilterTagDetails
427 stub -noname acmFilterTagEnum
428 stub -noname acmFormatChoose
429 stub -noname acmFormatDetails
430 stub -noname acmFormatEnum
431 stub -noname acmFormatSuggest
432 stub -noname acmFormatTagDetails
433 stub -noname acmFormatTagEnum
434 stub -noname acmStreamClose
435 stub -noname acmStreamConvert
436 stub -noname acmStreamMessage
437 stub -noname acmStreamOpen
438 stub -noname acmStreamPrepareHeader
439 stub -noname acmStreamReset
440 stub -noname acmStreamSize
441 stub -noname acmStreamUnprepareHeader
442 stub -noname acmGetVersion
443 stub -noname acmMetrics
444 stdcall WNetAddConnection3W(long ptr wstr wstr long) unicows.WNetAddConnection3W
445 stdcall WNetCancelConnection2W(wstr long long) unicows.WNetCancelConnection2W
446 stdcall WNetConnectionDialog1W(ptr) unicows.WNetConnectionDialog1W
447 stub -noname WNetDisconnectDialog
448 stdcall WNetDisconnectDialog1W(ptr) unicows.WNetDisconnectDialog1W
449 stdcall WNetGetConnectionW(wstr ptr ptr) unicows.WNetGetConnectionW
450 stdcall WNetGetUniversalNameW(wstr long ptr ptr) unicows.WNetGetUniversalNameW
451 stdcall WNetGetUserW(wstr wstr ptr) unicows.WNetGetUserW
452 stdcall WNetOpenEnumW(long long long ptr ptr) unicows.WNetOpenEnumW
453 stub -noname WNetCloseEnum
454 stdcall WNetEnumResourceW(long ptr ptr ptr) unicows.WNetEnumResourceW
455 stdcall RegCloseKey(long) kernel32.RegCloseKey
456 stdcall RegCreateKeyExW(long wstr long ptr long long ptr ptr ptr) kernel32.RegCreateKeyExW
457 stdcall RegDeleteKeyW(long wstr) unicows.RegDeleteKeyW
458 stdcall RegDeleteValueW(long wstr) kernel32.RegDeleteValueW
459 stdcall RegEnumValueW(long long ptr ptr ptr ptr ptr ptr) kernel32.RegEnumValueW
460 stdcall RegEnumKeyExW(long long ptr ptr ptr ptr ptr ptr) kernel32.RegEnumKeyExW
461 stdcall RegOpenKeyExW(long wstr long long ptr) kernel32.RegOpenKeyExW
462 stdcall RegQueryInfoKeyW(long ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr) kernel32.RegQueryInfoKeyW
463 stdcall RegQueryValueExW(long wstr ptr ptr ptr ptr) kernel32.RegQueryValueExW
464 stdcall RegSetValueExW(long wstr long long ptr long) kernel32.RegSetValueExW
465 stub -noname RegCopyFile
466 stub -noname RegRestoreFile
467 stub -noname PegSetUserNotification
468 stub -noname PegClearUserNotification
469 stub -noname PegRunAppAtTime
470 stub -noname PegRunAppAtEvent
471 stub -noname PegHandleAppNotifications
472 stub -noname PegGetUserNotificationPreferences
473 stub -noname CeSetUserNotification
474 stub -noname CeClearUserNotification
475 stub -noname CeRunAppAtTime
476 stub -noname CeRunAppAtEvent
477 stub -noname CeHandleAppNotifications
478 stub -noname CeGetUserNotificationPreferences
479 stub -noname CeEventHasOccurred
480 stub -noname ShellExecuteEx
481 stub -noname Shell_NotifyIcon
482 stub -noname SHGetFileInfo
483 stub -noname SHAddToRecentDocs
484 stub -noname SHCreateShortcut
485 stub -noname SHGetShortcutTarget
486 stub -noname SHShowOutOfMemory
487 stub -noname SHLoadDIBitmap
488 stdcall GetOpenFileNameW(ptr) unicows.GetOpenFileNameW
489 stdcall GetSaveFileNameW(ptr) unicows.GetSaveFileNameW
490 stub -noname QueryAPISetID
491 stdcall TerminateThread(long long) kernel32.TerminateThread
492 stdcall CreateThread(ptr long ptr long long ptr) kernel32.CreateThread
493 stdcall CreateProcessW(wstr wstr ptr ptr long long ptr wstr ptr ptr) kernel32.CreateProcessW
494 stub -noname EventModify
495 stdcall CreateEventW(ptr long long wstr) kernel32.CreateEventW
496 stdcall Sleep(long) kernel32.Sleep
497 stdcall WaitForSingleObject(long long) kernel32.WaitForSingleObject
498 stdcall WaitForMultipleObjects(long ptr long long) kernel32.WaitForMultipleObjects
499 stdcall SuspendThread(long) kernel32.SuspendThread
500 stdcall ResumeThread(long) kernel32.ResumeThread
502 stdcall SetThreadContext(long ptr) kernel32.SetThreadContext
503 stdcall WaitForDebugEvent(ptr long) kernel32.WaitForDebugEvent
504 stdcall ContinueDebugEvent(long long long) kernel32.ContinueDebugEvent
505 stdcall DebugActiveProcess(long) kernel32.DebugActiveProcess
506 stdcall ReadProcessMemory(long ptr ptr long ptr) kernel32.ReadProcessMemory
507 stdcall WriteProcessMemory(long ptr ptr long ptr) kernel32.WriteProcessMemory
508 stdcall FlushInstructionCache(long long long) kernel32.FlushInstructionCache
509 stdcall OpenProcess(long long long) kernel32.OpenProcess
510 stub -noname DumpKCallProfile
511 stub -noname THCreateSnapshot
512 stub -noname THGrow
513 stub -noname NotifyForceCleanboot
514 stdcall SetThreadPriority(long long) kernel32.SetThreadPriority
515 stdcall GetThreadPriority(long) kernel32.GetThreadPriority
516 stdcall GetLastError() kernel32.GetLastError
517 stdcall SetLastError(long) kernel32.SetLastError
518 stdcall GetExitCodeThread(long ptr) kernel32.GetExitCodeThread
519 stdcall GetExitCodeProcess(long ptr) kernel32.GetExitCodeProcess
520 stub -noname TlsCall
521 stdcall IsBadCodePtr(ptr) kernel32.IsBadCodePtr
522 stdcall IsBadReadPtr(ptr long) kernel32.IsBadReadPtr
523 stdcall IsBadWritePtr(ptr long) kernel32.IsBadWritePtr
524 stdcall VirtualAlloc(ptr long long long) kernel32.VirtualAlloc
525 stdcall VirtualFree(ptr long long) kernel32.VirtualFree
526 stdcall VirtualProtect(ptr long long ptr) kernel32.VirtualProtect
527 stdcall VirtualQuery(ptr ptr long) kernel32.VirtualQuery
528 stdcall LoadLibraryW(wstr) kernel32.LoadLibraryW
529 stdcall FreeLibrary(long) kernel32.FreeLibrary
530 stdcall GetProcAddressW(long wstr) COREDLL_GetProcAddressW
531 stub -noname FindResource
532 stdcall FindResourceW(long wstr wstr) kernel32.FindResourceW
533 stdcall LoadResource(long long) kernel32.LoadResource
534 stdcall SizeofResource(long long) kernel32.SizeofResource
535 stdcall GetTickCount() kernel32.GetTickCount
536 stdcall GetProcessVersion(long) kernel32.GetProcessVersion
537 stdcall GetModuleFileNameW(long ptr long) kernel32.GetModuleFileNameW
538 stdcall QueryPerformanceCounter(ptr) kernel32.QueryPerformanceCounter
539 stdcall QueryPerformanceFrequency(ptr) kernel32.QueryPerformanceFrequency
540 stub -noname ForcePageout
541 stdcall OutputDebugStringW(wstr) kernel32.OutputDebugStringW
542 stdcall GetSystemInfo(ptr) kernel32.GetSystemInfo
543 stdcall RaiseException(long long long ptr) kernel32.RaiseException
544 stdcall TerminateProcess(long long) kernel32.TerminateProcess
545 stub -noname NKDbgPrintfW
546 stub -noname RegisterDbgZones
547 stub -noname SetDaylightTime
548 stdcall CreateFileMappingW(long ptr long long long wstr) kernel32.CreateFileMappingW
549 stdcall MapViewOfFile(long long long long long) kernel32.MapViewOfFile
550 stdcall UnmapViewOfFile(ptr) kernel32.UnmapViewOfFile
551 stdcall FlushViewOfFile(ptr long) kernel32.FlushViewOfFile
552 stub -noname CreateFileForMapping
553 stdcall CloseHandle(long) kernel32.CloseHandle
555 stdcall CreateMutexW(ptr long wstr) kernel32.CreateMutexW
556 stdcall ReleaseMutex(long) kernel32.ReleaseMutex
557 stub -noname KernelIoControl
558 stub -noname AddEventAccess
559 stub -noname CreateAPISet
560 stub -noname VirtualCopy
563 stub -noname U_ropen
564 stub -noname U_rread
565 stub -noname U_rwrite
566 stub -noname U_rlseek
567 stub -noname U_rclose
568 stub -noname NKvDbgPrintfW
569 stub -noname ProfileSyscall
570 stub -noname GetRealTime
571 stub -noname SetRealTime
572 stub -noname ProcessDetachAllDLLs
573 stub -noname ExtractResource
574 stub -noname KernExtractIcons
575 stub -noname GetRomFileInfo
576 stub -noname GetRomFileBytes
577 stub -noname CacheSync
578 stub -noname AddTrackedItem
579 stub -noname DeleteTrackedItem
580 stub -noname PrintTrackedItem
581 stub -noname GetKPhys
582 stub -noname GiveKPhys
583 stub -noname SetExceptionHandler
584 stub -noname RegisterTrackedItem
585 stub -noname FilterTrackedItem
586 stub -noname SetKernelAlarm
587 stub -noname RefreshKernelAlarm
589 stub -noname CloseProcOE
590 stub -noname SetGwesOOMEvent
591 stub -noname StringCompress
592 stub -noname StringDecompress
593 stub -noname BinaryCompress
594 stub -noname BinaryDecompress
595 stub -noname InputDebugCharW
596 stub -noname TakeCritSec
597 stub -noname LeaveCritSec
598 stub -noname MapPtrToProcess
599 stub -noname MapPtrUnsecure
600 stub -noname GetProcFromPtr
601 stub -noname IsBadPtr
602 stub -noname GetProcAddrBits
603 stub -noname GetFSHeapInfo
604 stub -noname OtherThreadsRunning
605 stub -noname KillAllOtherThreads
606 stub -noname GetOwnerProcess
607 stub -noname GetCallerProcess
608 stub -noname GetIdleTime
609 stub -noname SetLowestScheduledPriority
610 stub -noname IsPrimaryThread
611 stub -noname SetProcPermissions
612 stub -noname GetCurrentPermissions
613 stub -noname IsEncryptionPermitted
614 stub -noname SetTimeZoneBias
615 stub -noname SetCleanRebootFlag
616 stub -noname CreateCrit
617 stub -noname PowerOffSystem
618 stub -noname SetDbgZone
619 stub -noname TurnOnProfiling
620 stub -noname TurnOffProfiling
621 stub -noname CeSetThreadPriority
622 stub -noname CeGetThreadPriority
623 stub -noname NKTerminateThread
624 stub -noname GetProcName
625 stub -noname SetHandleOwner
626 stub -noname LoadDriver
627 stub -noname InterruptInitialize
628 stub -noname InterruptDone
629 stub -noname InterruptDisable
630 stub -noname SetKMode
631 stub -noname SetPowerOffHandler
632 stub -noname SetGwesPowerHandler
633 stub -noname ConnectDebugger
634 stub -noname SetHardwareWatch
635 stub -noname RegisterAPISet
636 stub -noname CreateAPIHandle
637 stub -noname VerifyAPIHandle
638 stub -noname PPSHRestart
639 stub -noname SignalStarted
640 stub -noname GetProcessIndexFromID
641 stub -noname GetCallerProcessIndex
642 stub -noname DebugNotify
643 stub -noname AFS_Unmount
644 stub -noname AFS_CreateDirectoryW
645 stub -noname AFS_RemoveDirectoryW
646 stub -noname AFS_GetFileAttributesW
647 stub -noname AFS_SetFileAttributesW
648 stub -noname AFS_CreateFileW
649 stub -noname AFS_DeleteFileW
650 stub -noname AFS_MoveFileW
651 stub -noname AFS_FindFirstFileW
652 stub -noname AFS_RegisterFileSystemFunction
653 varargs sscanf(str str) msvcrt.sscanf
654 stub -noname AFS_PrestoChangoFileName
655 stub -noname AFS_CloseAllFileHandles
656 stub -noname AFS_GetDiskFreeSpace
657 stub -noname AFS_NotifyMountedFS
658 stdcall CreateCaret(long long long long) user32.CreateCaret
659 stdcall DestroyCaret() user32.DestroyCaret
660 stdcall HideCaret(long) user32.HideCaret
661 stdcall ShowCaret(long) user32.ShowCaret
662 stdcall SetCaretPos(long long) user32.SetCaretPos
663 stdcall GetCaretPos(ptr) user32.GetCaretPos
664 stdcall SetCaretBlinkTime(long) user32.SetCaretBlinkTime
665 stdcall GetCaretBlinkTime() user32.GetCaretBlinkTime
666 stub -noname DisableCaretSystemWide
667 stub -noname EnableCaretSystemWide
668 stdcall OpenClipboard(long) user32.OpenClipboard
669 stdcall CloseClipboard() user32.CloseClipboard
670 stdcall GetClipboardOwner() user32.GetClipboardOwner
671 stdcall SetClipboardData(long long) user32.SetClipboardData
672 stdcall GetClipboardData(long) user32.GetClipboardData
673 stdcall RegisterClipboardFormatW(wstr) user32.RegisterClipboardFormatW
674 stdcall CountClipboardFormats() user32.CountClipboardFormats
675 stdcall EnumClipboardFormats(long) user32.EnumClipboardFormats
676 stdcall GetClipboardFormatNameW(long ptr long) user32.GetClipboardFormatNameW
677 stdcall EmptyClipboard() user32.EmptyClipboard
678 stdcall IsClipboardFormatAvailable(long) user32.IsClipboardFormatAvailable
679 stdcall GetPriorityClipboardFormat(ptr long) user32.GetPriorityClipboardFormat
680 stdcall GetOpenClipboardWindow() user32.GetOpenClipboardWindow
681 stub -noname GetClipboardDataAlloc
682 stdcall SetCursor(long) user32.SetCursor
683 stdcall LoadCursorW(long wstr) user32.LoadCursorW
684 stdcall CheckRadioButton(long long long long) user32.CheckRadioButton
685 stdcall SendDlgItemMessageW(long long long long long) user32.SendDlgItemMessageW
686 stdcall SetDlgItemTextW(long long wstr) user32.SetDlgItemTextW
687 stdcall GetDlgItemTextW(long long ptr long) user32.GetDlgItemTextW
688 stdcall CreateDialogIndirectParamW(long ptr long ptr long) user32.CreateDialogIndirectParamW
689 stdcall DefDlgProcW(long long long long) user32.DefDlgProcW
690 stdcall DialogBoxIndirectParamW(long ptr long ptr long) user32.DialogBoxIndirectParamW
691 stdcall EndDialog(long long) user32.EndDialog
692 stdcall GetDlgItem(long long) user32.GetDlgItem
693 stdcall GetDlgCtrlID(long) user32.GetDlgCtrlID
694 stdcall GetDialogBaseUnits() user32.GetDialogBaseUnits
695 stdcall GetDlgItemInt(long long ptr long) user32.GetDlgItemInt
696 stdcall GetNextDlgTabItem(long long long) user32.GetNextDlgTabItem
697 stdcall GetNextDlgGroupItem(long long long) user32.GetNextDlgGroupItem
698 stdcall IsDialogMessageW(long ptr) user32.IsDialogMessageW
699 stdcall MapDialogRect(long ptr) user32.MapDialogRect
700 stdcall SetDlgItemInt(long long long long) user32.SetDlgItemInt
701 stdcall GetForegroundWindow() user32.GetForegroundWindow
702 stdcall SetForegroundWindow(long) user32.SetForegroundWindow
703 stdcall SetActiveWindow(long) user32.SetActiveWindow
704 stdcall SetFocus(long) user32.SetFocus
705 stdcall GetFocus() user32.GetFocus
706 stdcall GetActiveWindow() user32.GetActiveWindow
707 stdcall GetCapture() user32.GetCapture
708 stdcall SetCapture(long) user32.SetCapture
709 stdcall ReleaseCapture() user32.ReleaseCapture
710 stub -noname SetKeyboardTarget
711 stub -noname GetKeyboardTarget
712 stub -noname ShellModalEnd
713 stub -noname BatteryGetLifeTimeInfo
714 stub -noname BatteryNotifyOfTimeChange
715 stub -noname GetSystemPowerStatusEx
716 stub -noname NotifyWinUserSystem
717 stdcall GetVersionExW(ptr) kernel32.GetVersionExW
718 stub -noname WriteFileWithSeek
719 varargs sprintf(ptr str) msvcrt.sprintf
720 stub -noname SystemMemoryLow
721 cdecl vfwprintf(ptr wstr ptr) msvcrt.vfwprintf
723 stdcall CreateIconIndirect(ptr) user32.CreateIconIndirect
725 stdcall DestroyIcon(long) user32.DestroyIcon
726 stdcall DrawIconEx(long long long long long long long long long) user32.DrawIconEx
727 stdcall ExtractIconExW(wstr long ptr ptr long) unicows.ExtractIconExW
728 stdcall LoadIconW(long wstr) user32.LoadIconW
729 varargs _snprintf(ptr long str) msvcrt._snprintf
730 stdcall LoadImageW(long wstr long long long long) user32.LoadImageW
731 stdcall ClipCursor(ptr) user32.ClipCursor
732 stdcall GetClipCursor(ptr) user32.GetClipCursor
733 stdcall GetCursor() user32.GetCursor
734 stdcall GetCursorPos(ptr) user32.GetCursorPos
735 varargs fwscanf(ptr wstr) msvcrt.fwscanf
736 stdcall SetCursorPos(long long) user32.SetCursorPos
737 stdcall ShowCursor(long) user32.ShowCursor
738 stdcall ImageList_Add(ptr long long) comctl32.ImageList_Add
739 stdcall ImageList_AddMasked(ptr long long) comctl32.ImageList_AddMasked
740 stdcall ImageList_BeginDrag(ptr long long long) comctl32.ImageList_BeginDrag
741 stub -noname ImageList_CopyDitherImage
742 stdcall ImageList_Create(long long long long long) comctl32.ImageList_Create
743 stdcall ImageList_Destroy(ptr) comctl32.ImageList_Destroy
744 stdcall ImageList_DragEnter(long long long) comctl32.ImageList_DragEnter
745 stdcall ImageList_DragLeave(long) comctl32.ImageList_DragLeave
746 stdcall ImageList_DragMove(long long) comctl32.ImageList_DragMove
747 stdcall ImageList_DragShowNolock(long) comctl32.ImageList_DragShowNolock
748 stdcall ImageList_Draw(ptr long long long long long) comctl32.ImageList_Draw
749 stdcall ImageList_DrawEx(ptr long long long long long long long long long) comctl32.ImageList_DrawEx
750 stdcall ImageList_DrawIndirect(ptr) comctl32.ImageList_DrawIndirect
751 stdcall ImageList_EndDrag() comctl32.ImageList_EndDrag
752 stdcall ImageList_GetBkColor(ptr) comctl32.ImageList_GetBkColor
753 stdcall ImageList_GetDragImage(ptr ptr) comctl32.ImageList_GetDragImage
754 stdcall ImageList_GetIcon(ptr long long) comctl32.ImageList_GetIcon
755 stdcall ImageList_GetIconSize(ptr ptr ptr) comctl32.ImageList_GetIconSize
756 stdcall ImageList_GetImageCount(ptr) comctl32.ImageList_GetImageCount
757 stdcall ImageList_GetImageInfo(ptr long ptr) comctl32.ImageList_GetImageInfo
758 stdcall ImageList_LoadImage(long str long long long long long) comctl32.ImageList_LoadImage
759 stdcall ImageList_Merge(ptr long ptr long long long) comctl32.ImageList_Merge
760 stdcall ImageList_Remove(ptr long) comctl32.ImageList_Remove
761 stdcall ImageList_Replace(ptr long long long) comctl32.ImageList_Replace
762 stdcall ImageList_ReplaceIcon(ptr long long) comctl32.ImageList_ReplaceIcon
763 stdcall ImageList_SetBkColor(ptr long) comctl32.ImageList_SetBkColor
764 stdcall ImageList_SetDragCursorImage(ptr long long long) comctl32.ImageList_SetDragCursorImage
765 stdcall ImageList_SetIconSize(ptr long long) comctl32.ImageList_SetIconSize
766 stdcall ImageList_SetOverlayImage(ptr long long) comctl32.ImageList_SetOverlayImage
767 stdcall ImageList_Copy(ptr long ptr long long) comctl32.ImageList_Copy
768 stdcall ImageList_Duplicate(ptr) comctl32.ImageList_Duplicate
769 stdcall ImageList_SetImageCount(ptr long) comctl32.ImageList_SetImageCount
770 stdcall ImmAssociateContext(long long) imm32.ImmAssociateContext
771 stdcall ImmConfigureIMEW(long long long ptr) imm32.ImmConfigureIMEW
772 stdcall ImmCreateIMCC(long) imm32.ImmCreateIMCC
773 stdcall ImmDestroyIMCC(long) imm32.ImmDestroyIMCC
774 stdcall ImmEnumRegisterWordW(long ptr wstr long wstr ptr) imm32.ImmEnumRegisterWordW
775 stdcall ImmEscapeW(long long long ptr) imm32.ImmEscapeW
776 stdcall ImmGenerateMessage(ptr) imm32.ImmGenerateMessage
777 stdcall ImmGetCandidateListW(long long ptr long) imm32.ImmGetCandidateListW
778 stdcall ImmGetCandidateListCountW(long ptr) imm32.ImmGetCandidateListCountW
779 stdcall ImmGetCandidateWindow(long long ptr) imm32.ImmGetCandidateWindow
780 stdcall ImmGetCompositionFontW(long ptr) imm32.ImmGetCompositionFontW
781 stdcall ImmGetCompositionStringW(long long ptr long) imm32.ImmGetCompositionStringW
782 stdcall ImmGetCompositionWindow(long ptr) imm32.ImmGetCompositionWindow
783 stdcall ImmGetContext(long) imm32.ImmGetContext
784 stdcall ImmGetConversionListW(long long wstr ptr long long) imm32.ImmGetConversionListW
785 stdcall ImmGetConversionStatus(long ptr ptr) imm32.ImmGetConversionStatus
786 stdcall ImmGetDefaultIMEWnd(long) imm32.ImmGetDefaultIMEWnd
787 stdcall ImmGetDescriptionW(long ptr long) imm32.ImmGetDescriptionW
788 stdcall ImmGetGuideLineW(long long ptr long) imm32.ImmGetGuideLineW
789 stdcall ImmGetIMCCLockCount(long) imm32.ImmGetIMCCLockCount
790 stdcall ImmGetIMCCSize(long) imm32.ImmGetIMCCSize
791 stdcall ImmGetIMCLockCount(long) imm32.ImmGetIMCLockCount
792 stdcall ImmGetOpenStatus(long) imm32.ImmGetOpenStatus
793 stdcall ImmGetProperty(long long) imm32.ImmGetProperty
794 stdcall ImmGetRegisterWordStyleW(long long ptr) imm32.ImmGetRegisterWordStyleW
796 stdcall ImmIsUIMessageW(long long long long) imm32.ImmIsUIMessageW
797 stdcall ImmLockIMC(long) imm32.ImmLockIMC
798 stdcall ImmLockIMCC(long) imm32.ImmLockIMCC
800 stdcall ImmNotifyIME(long long long long) imm32.ImmNotifyIME
801 stdcall ImmReSizeIMCC(long long) imm32.ImmReSizeIMCC
802 stdcall ImmRegisterWordW(long wstr long wstr) imm32.ImmRegisterWordW
803 stdcall ImmReleaseContext(long long) imm32.ImmReleaseContext
804 stub -noname ImmSIPanelState
806 stub -noname ?ImmSetActiveContext@@YAHPAUHWND__@@KH@Z
807 stdcall ImmSetCandidateWindow(long ptr) imm32.ImmSetCandidateWindow
808 stdcall ImmSetCompositionFontW(long ptr) imm32.ImmSetCompositionFontW
809 stdcall ImmSetCompositionStringW(long long ptr long ptr long) imm32.ImmSetCompositionStringW
810 stdcall ImmSetCompositionWindow(long ptr) imm32.ImmSetCompositionWindow
811 stdcall ImmSetConversionStatus(long long long) imm32.ImmSetConversionStatus
812 stub -noname ImmSetHotKey
813 stdcall ImmGetHotKey(long ptr ptr ptr) imm32.ImmGetHotKey
814 stdcall ImmSetOpenStatus(long long) imm32.ImmSetOpenStatus
815 stdcall ImmSetStatusWindowPos(long ptr) imm32.ImmSetStatusWindowPos
816 stdcall ImmSimulateHotKey(long long) imm32.ImmSimulateHotKey
817 stdcall ImmUnlockIMC(long) imm32.ImmUnlockIMC
818 stdcall ImmUnlockIMCC(long) imm32.ImmUnlockIMCC
819 stdcall ImmUnregisterWordW(long wstr long wstr) imm32.ImmUnregisterWordW
820 stub -noname GetMouseMovePoints
821 stub -noname QASetWindowsJournalHook
822 stub -noname QAUnhookWindowsJournalHook
823 stdcall SendInput(long ptr long) user32.SendInput
824 stdcall mouse_event(long long long long long) user32.mouse_event
825 stub -noname EnableHardwareKeyboard
826 stdcall GetAsyncKeyState(long) user32.GetAsyncKeyState
827 stub -noname GetKeyboardStatus
828 stub -noname KeybdGetDeviceInfo
829 stub -noname KeybdInitStates
830 stub -noname KeybdVKeyToUnicode
831 stdcall MapVirtualKeyW(long long) user32.MapVirtualKeyW
832 stub -noname PostKeybdMessage
833 stdcall keybd_event(long long long long) user32.keybd_event
834 stub -noname GetAsyncShiftFlags
835 stdcall RegisterHotKey(long long long long) user32.RegisterHotKey
836 stdcall UnregisterHotKey(long long) user32.UnregisterHotKey
837 stub -noname SystemIdleTimerReset
838 stdcall TranslateAcceleratorW(long long ptr) user32.TranslateAcceleratorW
839 stub -noname NLedGetDeviceInfo
840 stub -noname NLedSetDevice
841 stdcall InsertMenuW(long long long long ptr) user32.InsertMenuW
842 stdcall AppendMenuW(long long long ptr) user32.AppendMenuW
843 stdcall RemoveMenu(long long long) user32.RemoveMenu
844 stdcall DestroyMenu(long) user32.DestroyMenu
845 stdcall TrackPopupMenuEx(long long long long long ptr) user32.TrackPopupMenuEx
846 stdcall LoadMenuW(long wstr) user32.LoadMenuW
847 stdcall EnableMenuItem(long long long) user32.EnableMenuItem
848 stdcall CheckMenuItem(long long long) user32.CheckMenuItem
849 stdcall CheckMenuRadioItem(long long long long long) user32.CheckMenuRadioItem
850 stdcall DeleteMenu(long long long) user32.DeleteMenu
851 stdcall CreateMenu() user32.CreateMenu
852 stdcall CreatePopupMenu() user32.CreatePopupMenu
853 stdcall SetMenuItemInfoW(long long long ptr) user32.SetMenuItemInfoW
854 stdcall GetMenuItemInfoW(long long long ptr) user32.GetMenuItemInfoW
855 stdcall GetSubMenu(long long) user32.GetSubMenu
856 stdcall DrawMenuBar(long) user32.DrawMenuBar
857 stdcall MessageBeep(long) user32.MessageBeep
858 stdcall MessageBoxW(long wstr wstr long) user32.MessageBoxW
859 stdcall -register DispatchMessageW(ptr) user32.DispatchMessageW
860 stdcall GetKeyState(long) user32.GetKeyState
861 stdcall -register GetMessageW(ptr long long long) user32.GetMessageW
862 stdcall GetMessagePos() user32.GetMessagePos
863 stub -noname GetMessageWNoWait
864 stdcall PeekMessageW(ptr long long long long) user32.PeekMessageW
865 stdcall PostMessageW(long long long long) user32.PostMessageW
866 stdcall PostQuitMessage(long) user32.PostQuitMessage
867 varargs fwprintf(ptr wstr) msvcrt.fwprintf
868 stdcall SendMessageW(long long long long) user32.SendMessageW
869 stdcall SendNotifyMessageW(long long long long) user32.SendNotifyMessageW
870 stdcall -register TranslateMessage(ptr) user32.TranslateMessage
871 stdcall MsgWaitForMultipleObjectsEx(long ptr long long long) user32.MsgWaitForMultipleObjectsEx
872 stub -noname GetMessageSource
873 stdcall LoadBitmapW(long wstr) user32.LoadBitmapW
874 stdcall LoadStringW(long long ptr long) user32.LoadStringW
875 stdcall SetTimer(long long long ptr) user32.SetTimer
876 stdcall KillTimer(long long) user32.KillTimer
877 stub -noname TouchCalibrate
878 stdcall GetClassInfoW(long wstr ptr) user32.GetClassInfoW
879 stdcall GetClassLongW(long long) user32.GetClassLongW
880 stdcall SetClassLongW(long long long) user32.SetClassLongW
881 stub -noname GetClassLong
882 stub -noname SetClassLong
884 stdcall UnregisterClassW(wstr long) user32.UnregisterClassW
885 stdcall GetSystemMetrics(long) user32.GetSystemMetrics
886 stdcall IsWindowVisible(long) user32.IsWindowVisible
887 stdcall AdjustWindowRectEx(ptr long long long) user32.AdjustWindowRectEx
888 stdcall GetDoubleClickTime() user32.GetDoubleClickTime
889 stdcall GetSysColor(long) user32.GetSysColor
890 stdcall SetSysColors(long ptr ptr) user32.SetSysColors
891 stdcall RegisterWindowMessageW(wstr) user32.RegisterWindowMessageW
892 stub -noname RegisterTaskBar
893 stdcall AddFontResourceW(wstr) gdi32.AddFontResourceW
894 stub -noname CeRemoveFontResource
895 stdcall CreateFontIndirectW(ptr) gdi32.CreateFontIndirectW
896 stdcall ExtTextOutW(long long long long ptr wstr long ptr) gdi32.ExtTextOutW
897 stdcall GetTextExtentExPointW(long wstr long long ptr ptr ptr) gdi32.GetTextExtentExPointW
898 stdcall GetTextMetricsW(long ptr) gdi32.GetTextMetricsW
899 stub -noname PegRemoveFontResource
900 stdcall RemoveFontResourceW(wstr) gdi32.RemoveFontResourceW
901 stdcall CreateBitmap(long long long long ptr) gdi32.CreateBitmap
902 stdcall CreateCompatibleBitmap(long long long) gdi32.CreateCompatibleBitmap
903 stdcall BitBlt(long long long long long long long long long) gdi32.BitBlt
904 stdcall MaskBlt(long long long long long long long long long long long long) gdi32.MaskBlt
905 stdcall StretchBlt(long long long long long long long long long long long) gdi32.StretchBlt
906 stub -noname TransparentImage
907 stdcall RestoreDC(long long) gdi32.RestoreDC
908 stdcall SaveDC(long) gdi32.SaveDC
909 stdcall CreateDCW(wstr wstr wstr ptr) gdi32.CreateDCW
910 stdcall CreateCompatibleDC(long) gdi32.CreateCompatibleDC
911 stdcall DeleteDC(long) gdi32.DeleteDC
912 stdcall DeleteObject(long) gdi32.DeleteObject
913 stdcall GetBkColor(long) gdi32.GetBkColor
914 stdcall GetBkMode(long) gdi32.GetBkMode
915 stdcall GetCurrentObject(long long) gdi32.GetCurrentObject
916 stdcall GetDeviceCaps(long long) gdi32.GetDeviceCaps
917 stdcall GetObjectType(long) gdi32.GetObjectType
918 stdcall GetObjectW(long long ptr) gdi32.GetObjectW
919 stdcall GetStockObject(long) gdi32.GetStockObject
920 stdcall GetTextColor(long) gdi32.GetTextColor
921 stdcall SelectObject(long long) gdi32.SelectObject
922 stdcall SetBkColor(long long) gdi32.SetBkColor
923 stdcall SetBkMode(long long) gdi32.SetBkMode
924 stdcall SetTextColor(long long) gdi32.SetTextColor
925 stdcall CreatePatternBrush(long) gdi32.CreatePatternBrush
926 stdcall CreatePen(long long long) gdi32.CreatePen
927 stdcall FillRgn(long long long) gdi32.FillRgn
928 stdcall SetROP2(long long) gdi32.SetROP2
929 stdcall CreateDIBPatternBrushPt(long long) gdi32.CreateDIBPatternBrushPt
930 stdcall CreatePenIndirect(ptr) gdi32.CreatePenIndirect
931 stdcall CreateSolidBrush(long) gdi32.CreateSolidBrush
932 stdcall DrawEdge(long ptr long long) user32.DrawEdge
933 stdcall DrawFocusRect(long ptr) user32.DrawFocusRect
934 stdcall Ellipse(long long long long long) gdi32.Ellipse
935 stdcall FillRect(long ptr long) user32.FillRect
936 stdcall GetPixel(long long long) gdi32.GetPixel
937 stdcall GetSysColorBrush(long) user32.GetSysColorBrush
938 stdcall PatBlt(long long long long long long) gdi32.PatBlt
939 stdcall Polygon(long ptr long) gdi32.Polygon
940 stdcall Polyline(long ptr long) gdi32.Polyline
941 stdcall Rectangle(long long long long long) gdi32.Rectangle
942 stdcall RoundRect(long long long long long long long) gdi32.RoundRect
943 stdcall SetBrushOrgEx(long long long ptr) gdi32.SetBrushOrgEx
944 stdcall SetPixel(long long long long) gdi32.SetPixel
945 stdcall DrawTextW(long wstr long ptr long) user32.DrawTextW
946 stub -noname CreateBitmapFromPointer
947 stdcall CreatePalette(ptr) gdi32.CreatePalette
948 stdcall GetNearestPaletteIndex(long long) gdi32.GetNearestPaletteIndex
949 stdcall GetPaletteEntries(long long long ptr) gdi32.GetPaletteEntries
950 stdcall GetSystemPaletteEntries(long long long ptr) gdi32.GetSystemPaletteEntries
951 stdcall SetPaletteEntries(long long long ptr) gdi32.SetPaletteEntries
952 stdcall GetNearestColor(long long) gdi32.GetNearestColor
953 stdcall RealizePalette(long) gdi32.RealizePalette
954 stdcall SelectPalette(long long long) gdi32.SelectPalette
955 stdcall AbortDoc(long) gdi32.AbortDoc
956 stub -noname ?CloseEnhMetaFile@@YAPAUHENHMETAFILE__@@PAUHDC__@@@Z
957 stub -noname ?CreateEnhMetaFileW@@YAPAUHDC__@@PAU1@PBGPBUtagRECT@@1@Z
958 stub -noname ?DeleteEnhMetaFile@@YAHPAUHENHMETAFILE__@@@Z
959 stdcall EndDoc(long) gdi32.EndDoc
960 stdcall EndPage(long) gdi32.EndPage
961 stub -noname ?PlayEnhMetaFile@@YAHPAUHDC__@@PAUHENHMETAFILE__@@PBUtagRECT@@@Z
962 stdcall SetAbortProc(long ptr) gdi32.SetAbortProc
963 stdcall StartDocW(long ptr) gdi32.StartDocW
964 stdcall StartPage(long) gdi32.StartPage
965 stdcall EnumFontFamiliesW(long wstr ptr long) gdi32.EnumFontFamiliesW
966 stdcall EnumFontsW(long wstr ptr long) gdi32.EnumFontsW
967 stdcall GetTextFaceW(long long ptr) gdi32.GetTextFaceW
968 stdcall CombineRgn(long long long long) gdi32.CombineRgn
969 stdcall CreateRectRgnIndirect(ptr) gdi32.CreateRectRgnIndirect
970 stdcall ExcludeClipRect(long long long long long) gdi32.ExcludeClipRect
971 stdcall GetClipBox(long ptr) gdi32.GetClipBox
972 stdcall GetClipRgn(long long) gdi32.GetClipRgn
973 stdcall GetRegionData(long long ptr) gdi32.GetRegionData
974 stdcall GetRgnBox(long ptr) gdi32.GetRgnBox
975 stdcall IntersectClipRect(long long long long long) gdi32.IntersectClipRect
976 stdcall OffsetRgn(long long long) gdi32.OffsetRgn
977 stdcall PtInRegion(long long long) gdi32.PtInRegion
978 stdcall RectInRegion(long ptr) gdi32.RectInRegion
979 stdcall SelectClipRgn(long long) gdi32.SelectClipRgn
980 stdcall CreateRectRgn(long long long long) gdi32.CreateRectRgn
981 stdcall RectVisible(long ptr) gdi32.RectVisible
982 stdcall SetRectRgn(long long long long long) gdi32.SetRectRgn
983 stdcall SetViewportOrgEx(long long long ptr) gdi32.SetViewportOrgEx
984 stub -noname ?SetObjectOwner@@YAHPAX0@Z
985 stdcall ScrollDC(long long long ptr ptr long ptr) user32.ScrollDC
986 stdcall EnableEUDC(long) gdi32.EnableEUDC
987 stdcall DrawFrameControl(long ptr long long) user32.DrawFrameControl
988 cdecl abs(long) msvcrt.abs
989 cdecl acos(double) msvcrt.acos
990 cdecl asin(double) msvcrt.asin
991 cdecl atan(double) msvcrt.atan
992 cdecl atan2(double double) msvcrt.atan2
993 cdecl atoi(str) msvcrt.atoi
994 cdecl atol(str) msvcrt.atol
995 cdecl atof(str) msvcrt.atof
996 cdecl _atodbl(ptr str) msvcrt._atodbl
997 stub -noname _atoflt
998 cdecl _cabs(long) msvcrt._cabs
999 cdecl ceil(double) msvcrt.ceil
1000 cdecl _chgsign(double) msvcrt._chgsign
1001 cdecl _clearfp() msvcrt._clearfp
1002 cdecl _controlfp(long long) msvcrt._controlfp
1003 cdecl _copysign(double double) msvcrt._copysign
1004 cdecl cos(double) msvcrt.cos
1005 cdecl cosh(double) msvcrt.cosh
1006 cdecl difftime(long long) msvcrt.difftime
1007 cdecl -ret64 div(long long) msvcrt.div
1008 cdecl _ecvt(double long ptr ptr) msvcrt._ecvt
1009 cdecl exp(double) msvcrt.exp
1010 cdecl fabs(double) msvcrt.fabs
1011 cdecl _fcvt(double long ptr ptr) msvcrt._fcvt
1012 cdecl _finite(double) msvcrt._finite
1013 cdecl floor(double) msvcrt.floor
1014 cdecl fmod(double double) msvcrt.fmod
1015 cdecl _fpclass(double) msvcrt._fpclass
1016 cdecl _fpieee_flt(long ptr ptr) msvcrt._fpieee_flt
1017 cdecl _fpreset() msvcrt._fpreset
1018 cdecl free(ptr) msvcrt.free
1019 cdecl frexp(double ptr) msvcrt.frexp
1020 stub -noname _frnd
1021 stub -noname _fsqrt
1022 cdecl _gcvt(double long str) msvcrt._gcvt
1023 cdecl _hypot(double double) msvcrt._hypot
1024 cdecl _isnan(double) msvcrt._isnan
1025 cdecl _itoa(long ptr long) msvcrt._itoa
1026 cdecl _itow(long ptr long) msvcrt._itow
1027 cdecl _j0(double) msvcrt._j0
1028 cdecl _j1(double) msvcrt._j1
1029 cdecl _jn(long double) msvcrt._jn
1030 cdecl labs(long) msvcrt.labs
1031 cdecl ldexp(double long) msvcrt.ldexp
1032 cdecl ldiv(long long) msvcrt.ldiv
1033 cdecl log(double) msvcrt.log
1034 cdecl log10(double) msvcrt.log10
1035 cdecl _logb(double) msvcrt._logb
1036 cdecl -arch=i386 longjmp(ptr long) msvcrt.longjmp
1037 cdecl _lrotl(long long) msvcrt._lrotl
1038 cdecl _lrotr(long long) msvcrt._lrotr
1039 cdecl _ltoa(long ptr long) msvcrt._ltoa
1040 cdecl _ltow(long ptr long) msvcrt._ltow
1041 cdecl malloc(long) msvcrt.malloc
1042 cdecl _memccpy(ptr ptr long long) msvcrt._memccpy
1043 cdecl memcmp(ptr ptr long) msvcrt.memcmp
1044 cdecl memcpy(ptr ptr long) msvcrt.memcpy
1045 cdecl _memicmp(str str long) msvcrt._memicmp
1046 cdecl memmove(ptr ptr long) msvcrt.memmove
1047 cdecl memset(ptr long long) msvcrt.memset
1048 cdecl modf(double ptr) msvcrt.modf
1049 cdecl _msize(ptr) msvcrt._msize
1050 cdecl _nextafter(double double) msvcrt._nextafter
1051 cdecl pow(double double) msvcrt.pow
1052 cdecl qsort(ptr long long ptr) msvcrt.qsort
1053 cdecl rand() msvcrt.rand
1054 cdecl realloc(ptr long) msvcrt.realloc
1055 cdecl _rotl(long long) msvcrt._rotl
1056 cdecl _rotr(long long) msvcrt._rotr
1057 cdecl _scalb(double long) msvcrt._scalb
1058 cdecl sin(double) msvcrt.sin
1059 cdecl sinh(double) msvcrt.sinh
1060 cdecl sqrt(double) msvcrt.sqrt
1061 cdecl srand(long) msvcrt.srand
1062 cdecl _statusfp() msvcrt._statusfp
1063 cdecl strcat(str str) msvcrt.strcat
1064 cdecl strchr(str long) msvcrt.strchr
1065 cdecl strcmp(str str) msvcrt.strcmp
1066 cdecl strcpy(ptr str) msvcrt.strcpy
1067 cdecl strcspn(str str) msvcrt.strcspn
1068 cdecl strlen(str) msvcrt.strlen
1069 cdecl strncat(str str long) msvcrt.strncat
1070 cdecl strncmp(str str long) msvcrt.strncmp
1071 cdecl strncpy(ptr str long) msvcrt.strncpy
1072 cdecl strstr(str str) msvcrt.strstr
1073 cdecl strtok(str str) msvcrt.strtok
1074 cdecl _swab(str str long) msvcrt._swab
1075 cdecl tan(double) msvcrt.tan
1076 cdecl tanh(double) msvcrt.tanh
1079 cdecl _ultoa(long ptr long) msvcrt._ultoa
1080 cdecl _ultow(long ptr long) msvcrt._ultow
1081 cdecl wcstod(wstr ptr) msvcrt.wcstod
1082 cdecl wcstol(wstr ptr long) msvcrt.wcstol
1083 cdecl wcstoul(wstr ptr long) msvcrt.wcstoul
1084 cdecl _y0(double) msvcrt._y0
1085 cdecl _y1(double) msvcrt._y1
1086 cdecl _yn(long double) msvcrt._yn
1087 stub -noname _ld12tod
1088 stub -noname _ld12tof
1089 stub -noname __strgtold12
1090 cdecl tolower(long) msvcrt.tolower
1091 cdecl toupper(long) msvcrt.toupper
1092 cdecl _purecall() msvcrt._purecall
1093 stub -noname _fltused
1094 cdecl -arch=win32 ??3@YAXPAX@Z(ptr) msvcrt.??3@YAXPAX@Z
1095 cdecl -arch=win32 ??2@YAPAXI@Z(long) msvcrt.??2@YAPAXI@Z
1096 varargs _snwprintf(ptr long wstr) msvcrt._snwprintf
1097 varargs swprintf(ptr wstr) msvcrt.swprintf
1098 varargs swscanf(wstr wstr) msvcrt.swscanf
1099 cdecl vswprintf(ptr wstr ptr) msvcrt.vswprintf
1100 stub -noname _getstdfilex
1101 varargs scanf(str) msvcrt.scanf
1102 varargs printf(str) msvcrt.printf
1103 cdecl vprintf(str ptr) msvcrt.vprintf
1104 cdecl getchar() msvcrt.getchar
1105 cdecl putchar(long) msvcrt.putchar
1106 cdecl gets(str) msvcrt.gets
1107 cdecl puts(str) msvcrt.puts
1108 cdecl fgetc(ptr) msvcrt.fgetc
1109 cdecl fgets(ptr long ptr) msvcrt.fgets
1110 cdecl fputc(long ptr) msvcrt.fputc
1111 cdecl fputs(str ptr) msvcrt.fputs
1112 cdecl ungetc(long ptr) msvcrt.ungetc
1113 cdecl fopen(str str) msvcrt.fopen
1114 varargs fscanf(ptr str) msvcrt.fscanf
1115 varargs fprintf(ptr str) msvcrt.fprintf
1116 cdecl vfprintf(ptr str ptr) msvcrt.vfprintf
1117 cdecl _wfdopen(long wstr) msvcrt._wfdopen
1118 cdecl fclose(ptr) msvcrt.fclose
1119 cdecl _fcloseall() msvcrt._fcloseall
1120 cdecl fread(ptr long long ptr) msvcrt.fread
1121 cdecl fwrite(ptr long long ptr) msvcrt.fwrite
1122 cdecl fflush(ptr) msvcrt.fflush
1123 cdecl _flushall() msvcrt._flushall
1124 cdecl _fileno(ptr) msvcrt._fileno
1125 cdecl feof(ptr) msvcrt.feof
1126 cdecl ferror(ptr) msvcrt.ferror
1127 cdecl clearerr(ptr) msvcrt.clearerr
1128 cdecl fgetpos(ptr ptr) msvcrt.fgetpos
1129 cdecl fsetpos(ptr ptr) msvcrt.fsetpos
1130 cdecl fseek(ptr long long) msvcrt.fseek
1131 cdecl ftell(ptr) msvcrt.ftell
1132 cdecl _vsnwprintf(ptr long wstr ptr) msvcrt._vsnwprintf
1133 varargs wscanf(wstr) msvcrt.wscanf
1134 varargs wprintf(wstr) msvcrt.wprintf
1135 cdecl vwprintf(wstr ptr) msvcrt.vwprintf
1136 cdecl getwchar() msvcrt.getwchar
1137 cdecl putwchar(long) msvcrt.putwchar
1138 cdecl _getws(ptr) msvcrt._getws
1139 cdecl _putws(wstr) msvcrt._putws
1140 cdecl fgetwc(ptr) msvcrt.fgetwc
1141 cdecl fputwc(long ptr) msvcrt.fputwc
1142 cdecl ungetwc(long ptr) msvcrt.ungetwc
1143 cdecl fgetws(ptr long ptr) msvcrt.fgetws
1144 cdecl fputws(wstr ptr) msvcrt.fputws
1145 cdecl _wfopen(wstr wstr) msvcrt._wfopen
1146 cdecl vsprintf(ptr str ptr) msvcrt.vsprintf
1147 cdecl _vsnprintf(ptr long str ptr) msvcrt._vsnprintf
1148 stdcall GetThreadContext(long ptr) kernel32.GetThreadContext
1149 stub -noname GetStdioPathW
1150 stub -noname SetStdioPathW
1151 stub -noname _InitStdioLib
1152 stdcall RegFlushKey(long) kernel32.RegFlushKey
1153 stub -noname ReadRegistryFromOEM
1154 stub -noname WriteRegistryToOEM
1155 stub -noname WriteDebugLED
1156 stub -noname UnregisterFunc1
1157 stdcall BeginDeferWindowPos(long) user32.BeginDeferWindowPos
1158 stdcall DeferWindowPos(long long long long long long long long) user32.DeferWindowPos
1159 stdcall EndDeferWindowPos(long) user32.EndDeferWindowPos
1160 stdcall GetKeyboardLayoutNameW(ptr) user32.GetKeyboardLayoutNameW
1161 stub -noname LockPages
1162 stub -noname UnlockPages
1163 stub -noname SHCreateExplorerInstance
1164 stub -noname CeMountDBVol
1165 stub -noname CeEnumDBVolumes
1166 stdcall TranslateCharsetInfo(ptr ptr long) gdi32.TranslateCharsetInfo
1167 stub -noname CreateFileForMappingW
1168 stub -noname lineSetCurrentLocation
1169 stub -noname SipStatus
1170 stub -noname SipRegisterNotification
1171 stub -noname SipShowIM
1172 stub -noname SipGetInfo
1173 stub -noname SipSetInfo
1174 stub -noname SipEnumIM
1175 stub -noname SipGetCurrentIM
1176 stub -noname SipSetCurrentIM
1177 stdcall GetModuleHandleW(wstr) kernel32.GetModuleHandleW
1179 stub -noname ActivateDevice
1180 stub -noname DeactivateDevice
1181 extern _HUGE msvcrt._HUGE
1182 stdcall ExtEscape(long long long ptr long ptr) gdi32.ExtEscape
1185 stdcall GetDCEx(long long long) user32.GetDCEx
1186 stdcall GetThreadTimes(long ptr ptr ptr ptr) kernel32.GetThreadTimes
1187 cdecl _setmode(long long) msvcrt._setmode
1189 stub -noname CeFindNextDatabaseEx
1190 stub -noname CeCreateDatabaseEx
1191 stub -noname CeSetDatabaseInfoEx
1192 stub -noname CeOpenDatabaseEx
1193 stub -noname CeDeleteDatabaseEx
1194 stub -noname CeReadRecordPropsEx
1195 stub -noname CeOidGetInfoEx
1196 stub -noname CeFindFirstDatabaseEx
1197 stub -noname CeUnmountDBVol
1198 stdcall ImmCreateContext() imm32.ImmCreateContext
1199 stdcall ImmDestroyContext(long) imm32.ImmDestroyContext
1200 stdcall ImmGetStatusWindowPos(long ptr) imm32.ImmGetStatusWindowPos
1201 cdecl _wfreopen(wstr wstr ptr) msvcrt._wfreopen
1202 stdcall SetWindowsHookExW(long long long long) user32.SetWindowsHookExW
1203 stdcall UnhookWindowsHookEx(long) user32.UnhookWindowsHookEx
1204 stdcall CallNextHookEx(long long long long) user32.CallNextHookEx
1205 stdcall ImmAssociateContextEx(long long long) imm32.ImmAssociateContextEx
1206 stdcall ImmDisableIME(long) imm32.ImmDisableIME
1207 stdcall ImmGetIMEFileNameW(long ptr long) imm32.ImmGetIMEFileNameW
1209 stdcall ImmIsIME(long) imm32.ImmIsIME
1210 stdcall ImmGetVirtualKey(long) imm32.ImmGetVirtualKey
1211 stdcall ImmGetImeMenuItemsW(long long long ptr ptr long) imm32.ImmGetImeMenuItemsW
1213 stub -noname IsProcessDying
1214 stub -noname SipSetDefaultRect
1215 stub -noname FlushViewOfFileMaybe
1216 stdcall FreeLibraryAndExitThread(long long) kernel32.FreeLibraryAndExitThread
1217 stub -noname CeFlushDBVol
1218 stub -noname ?DefaultImcGet@@YAKXZ
1219 stub -noname ?DefaultImeWndGet@@YAPAUHWND__@@XZ
1220 stub -noname ?ImmProcessKey@@YAKPAUHWND__@@IJKI@Z
1221 stub -noname ?ImmTranslateMessage@@YAHPAUHWND__@@IIJHIIPAH@Z
1222 stub -noname ImmSetImeWndIMC
1223 stub -noname ?ImmGetUIClassName@@YAPAGXZ
1224 stub -noname GetForegroundInfo
1225 stub -noname GetForegroundKeyboardTarget
1226 stub -noname CeFreeNotification
1227 stub -noname GetCRTStorageEx
1228 stub -noname GetCRTFlags
1229 stdcall GetKeyboardLayout(long) user32.GetKeyboardLayout
1230 stub -noname GetProcAddressA
1231 stdcall GetCommandLineW() kernel32.GetCommandLineW
1232 stdcall DisableThreadLibraryCalls(long) kernel32.DisableThreadLibraryCalls
1233 stdcall TryEnterCriticalSection(ptr) kernel32.TryEnterCriticalSection
1234 stdcall GetTempFileNameW(wstr wstr long ptr) kernel32.GetTempFileNameW
1235 stdcall FindFirstFileExW(wstr long ptr long ptr long) kernel32.FindFirstFileExW
1236 stub -noname GetDeviceByIndex
1237 stdcall GetFileAttributesExW(wstr long ptr) kernel32.GetFileAttributesExW
1238 stdcall CreateSemaphoreW(ptr long long wstr) kernel32.CreateSemaphoreW
1239 stdcall ReleaseSemaphore(long long ptr) kernel32.ReleaseSemaphore
1240 stub -noname ComThreadBaseFunc
1241 stdcall LoadLibraryExW(wstr long long) kernel32.LoadLibraryExW
1242 stdcall ImmRequestMessageW(ptr long long) imm32.ImmRequestMessageW
1244 stub -noname CeSetThreadQuantum
1245 stub -noname CeGetThreadQuantum
1246 stub -noname lineAccept
1247 stub -noname lineAddToConference
1248 stub -noname lineAnswer
1249 stub -noname lineBlindTransfer
1250 stub -noname lineCompleteTransfer
1251 stub -noname lineDevSpecific
1252 stub -noname lineDial
1253 stub -noname lineForward
1254 stub -noname lineGenerateDigits
1255 stub -noname lineGenerateTone
1256 stub -noname lineGetAddressCaps
1257 stub -noname lineGetAddressID
1258 stub -noname lineGetAddressStatus
1259 stub -noname lineGetAppPriority
1260 stub -noname lineGetCallInfo
1261 stub -noname lineGetCallStatus
1262 stub -noname lineGetConfRelatedCalls
1263 stub -noname lineGetIcon
1264 stub -noname lineGetLineDevStatus
1265 stub -noname lineGetMessage
1266 stub -noname lineGetNewCalls
1267 stub -noname lineGetNumRings
1268 stub -noname lineGetProviderList
1269 stub -noname lineGetStatusMessages
1270 stub -noname lineHandoff
1271 stub -noname lineHold
1272 stub -noname lineInitializeEx
1273 stub -noname lineMonitorDigits
1274 stub -noname lineMonitorMedia
1275 stub -noname lineNegotiateExtVersion
1276 stub -noname linePickup
1277 stub -noname linePrepareAddToConference
1278 stub -noname lineRedirect
1279 stub -noname lineReleaseUserUserInfo
1280 stub -noname lineRemoveFromConference
1281 stub -noname lineSendUserUserInfo
1282 stub -noname lineSetAppPriority
1283 stub -noname lineSetCallParams
1284 stub -noname lineSetCallPrivilege
1285 stub -noname lineSetMediaMode
1286 stub -noname lineSetNumRings
1287 stub -noname lineSetTerminal
1288 stub -noname lineSetTollList
1289 stub -noname lineSetupConference
1290 stub -noname lineSetupTransfer
1291 stub -noname lineSwapHold
1292 stub -noname lineUnhold
1293 stub -noname phoneClose
1294 stub -noname phoneConfigDialog
1295 stub -noname phoneDevSpecific
1296 stub -noname phoneGetDevCaps
1297 stub -noname phoneGetGain
1298 stub -noname phoneGetHookSwitch
1299 stub -noname phoneGetIcon
1300 stub -noname phoneGetID
1301 stub -noname phoneGetMessage
1302 stub -noname phoneGetRing
1303 stub -noname phoneGetStatus
1304 stub -noname phoneGetStatusMessages
1305 stub -noname phoneGetVolume
1306 stub -noname phoneInitializeEx
1307 stub -noname phoneNegotiateAPIVersion
1308 stub -noname phoneNegotiateExtVersion
1309 stub -noname phoneOpen
1310 stub -noname phoneSetGain
1311 stub -noname phoneSetHookSwitch
1312 stub -noname phoneSetRing
1313 stub -noname phoneSetStatusMessages
1314 stub -noname phoneSetVolume
1315 stub -noname phoneShutdown
1317 stdcall GetSystemDefaultUILanguage() kernel32.GetSystemDefaultUILanguage
1318 stdcall GetUserDefaultUILanguage() kernel32.GetUserDefaultUILanguage
1319 stub -noname SetUserDefaultUILanguage
1320 stdcall EnumUILanguagesW(ptr long long) kernel32.EnumUILanguagesW
1346 cdecl calloc(long long) msvcrt.calloc
1352 stub -noname CeSetUserNotificationEx
1353 stub -noname CeGetUserNotificationHandles
1354 stub -noname CeGetUserNotification
1357 stub -noname CeGetCurrentTrust
1358 stub -noname GetSystemPowerStatusEx2
1395 stub -noname CeGetCallerTrust
1396 stub -noname OpenDeviceKey
1397 stdcall GetDesktopWindow() user32.GetDesktopWindow
1398 stdcall SetWindowRgn(long long long) user32.SetWindowRgn
1399 stdcall GetWindowRgn(long long) user32.GetWindowRgn
1403 cdecl strtod(str ptr) msvcrt.strtod
1404 cdecl strtol(str ptr long) msvcrt.strtol
1405 cdecl strtoul(str ptr long) msvcrt.strtoul
1406 cdecl strpbrk(str str) msvcrt.strpbrk
1407 cdecl strrchr(str long) msvcrt.strrchr
1408 cdecl strspn(str str) msvcrt.strspn
1409 cdecl _strdup(str) msvcrt._strdup
1410 cdecl _stricmp(str str) msvcrt._stricmp
1411 cdecl _strnicmp(str str long) msvcrt._strnicmp
1412 cdecl _strnset(str long long) msvcrt._strnset
1413 cdecl _strrev(str) msvcrt._strrev
1414 cdecl _strset(str long) msvcrt._strset
1415 cdecl _strlwr(str) msvcrt._strlwr
1416 cdecl _strupr(str) msvcrt._strupr
1417 cdecl _isctype(long long) msvcrt._isctype
1418 cdecl -ret64 _atoi64(str) msvcrt._atoi64
1419 stdcall InSendMessage() user32.InSendMessage
1420 stdcall GetQueueStatus(long) user32.GetQueueStatus
1421 stub -noname LoadFSDEx
1424 stdcall RasEnumDevicesW(ptr ptr ptr) unicows.RasEnumDevicesW
1425 stub -noname CeResyncFilesys
1443 stub -noname CeGetRandomSeed
1446 stub -noname CeMapArgumentArray
1447 stub -noname UpdateNLSInfo
1448 stub -noname PerformCallBack4
1451 stub -noname CeLogData
1452 stub -noname CeLogSetZones
1453 stub -noname AllKeys
1454 stub -noname GetWindowTextWDirect
1455 stub -noname CeSetExtendedPdata
1456 cdecl -arch=win32 ??_U@YAPAXI@Z(long) msvcrt.??_U@YAPAXI@Z
1457 cdecl -arch=win32 ??_V@YAXPAX@Z(ptr) msvcrt.??_V@YAXPAX@Z
1458 stdcall RasGetProjectionInfoW(long long ptr ptr) unicows.RasGetProjectionInfoW
1459 stdcall VerQueryValueW(ptr wstr ptr ptr) version.VerQueryValueW
1460 stdcall GetFileVersionInfoW(wstr long long ptr) version.GetFileVersionInfoW
1461 stdcall GetFileVersionInfoSizeW(wstr ptr) version.GetFileVersionInfoSizeW
1462 stub -noname SetOOMEvent
1463 stub -noname RasGetLinkStatistics
1464 stub -noname RasGetDispPhoneNumW
1465 stub -noname RasDevConfigDialogEditW
1466 stub -noname CreateLocaleView
1467 stub -noname CeLogReSync
1468 stub -noname CeCreateDatabaseEx2
1469 stub -noname CeOpenDatabaseEx2
1470 stub -noname CeSeekDatabaseEx
1471 stub -noname CeSetDatabaseInfoEx2
1472 stub -noname CeOidGetInfoEx2
1473 stub -noname CeGetDBInformationByHandle
1474 stub -noname ThreadExceptionExit
1475 stub -noname LoadIntChainHandler
1476 stub -noname FreeIntChainHandler
1477 stub -noname GetMessageQueueReadyTimeStamp
1478 stub -noname RegSaveKey
1479 stub -noname RegReplaceKey
1486 stub -noname AllocPhysMem
1487 stub -noname FreePhysMem
1488 stub -noname SHCreateShortcutEx
1489 stub -noname KernelLibIoControl
1490 stub -noname RegisterAFSEx
1491 stdcall -arch=i386 InterlockedExchangeAdd(ptr long ) kernel32.InterlockedExchangeAdd
1492 stdcall -arch=i386 InterlockedCompareExchange(ptr long long) kernel32.InterlockedCompareExchange
1493 stub -noname LoadAnimatedCursor
1494 stub -noname ActivateDeviceEx
1495 stub -noname SendMessageTimeout
1496 stdcall OpenEventW(long long wstr) kernel32.OpenEventW
1497 stub -noname SetProp
1498 stub -noname GetProp
1499 stub -noname RemoveProp
1500 stub -noname EnumPropsEx
1501 stub -noname SetCurrentUser
1502 stub -noname SetUserData
1503 stub -noname GetUserNameExW
1504 stub -noname RequestDeviceNotifications
1505 stub -noname StopDeviceNotifications
1506 stub -noname RegisterTaskBarEx
1507 stub -noname RegisterDesktop
1508 stub -noname ActivateService
1509 stub -noname RegisterService
1510 stub -noname DeregisterService
1511 stub -noname CloseAllServiceHandles
1512 stub -noname CreateServiceHandle
1513 stub -noname GetServiceByIndex
1514 stub -noname ServiceIoControl
1515 stub -noname ServiceAddPort
1516 stub -noname ServiceUnbindPorts
1517 stub -noname EnumServices
1518 stub -noname GetServiceHandle
1519 stdcall GlobalAddAtomW(wstr) kernel32.GlobalAddAtomW
1520 stdcall GlobalDeleteAtom(long) kernel32.GlobalDeleteAtom
1521 stdcall GlobalFindAtomW(wstr) kernel32.GlobalFindAtomW
1522 stdcall MonitorFromPoint(int64 long) user32.MonitorFromPoint
1523 stdcall MonitorFromRect(ptr long) user32.MonitorFromRect
1524 stdcall MonitorFromWindow(long long) user32.MonitorFromWindow
1525 stub -noname GetMonitorInfo
1526 stdcall EnumDisplayMonitors(long ptr ptr long) user32.EnumDisplayMonitors
1527 stub -noname GetEventData
1528 stub -noname SetEventData
1529 stub -noname CreateMsgQueue
1530 stub -noname ReadMsgQueue
1531 stub -noname WriteMsgQueue
1532 stub -noname GetMsgQueueInfo
1533 stub -noname CloseMsgQueue
1534 stub -noname SleepTillTick
1535 stdcall DuplicateHandle(long long long ptr long long long) kernel32.DuplicateHandle
1536 stub -noname OpenMsgQueue
1537 stub -noname SetPasswordStatus
1538 stub -noname GetPasswordStatus
1539 stub -noname CreateStaticMapping
1540 stub -noname AccessibilitySoundSentryEvent
1541 stub -noname ImmEnableIME
1542 stub -noname RegOpenProcessKey
1550 cdecl -arch=i386 -norelay __CxxFrameHandler(ptr ptr ptr ptr) msvcrt.__CxxFrameHandler
1551 stub -noname __CxxThrowException
1552 stub -noname ?set_terminate@std@@YAP6AXXZP6AXXZ@Z
1553 stub -noname ?set_unexpected@std@@YAP6AXXZP6AXXZ@Z
1555 stub -noname ?__set_inconsistency@@YAP6AXXZP6AXXZ@Z
1556 stub -noname ?terminate@std@@YAXXZ
1557 stub -noname ?unexpected@std@@YAXXZ
1558 stub -noname ?_inconsistency@@YAXXZ
1559 cdecl __RTCastToVoid(ptr) msvcrt.__RTCastToVoid
1560 cdecl __RTtypeid(ptr) msvcrt.__RTtypeid
1561 cdecl __RTDynamicCast(ptr long ptr ptr long) msvcrt.__RTDynamicCast
1562 stub -noname ??1type_info@@UAA@XZ
1563 stub -noname ??8type_info@@QBAHABV0@@Z
1564 stub -noname ??9type_info@@QBAHABV0@@Z
1565 stub -noname ?before@type_info@@QBAHABV1@@Z
1566 stub -noname ?name@type_info@@QBAPBDXZ
1567 stub -noname ?raw_name@type_info@@QBAPBDXZ
1568 stub -noname ??0type_info@@AAA@ABV0@@Z
1569 stub -noname ??4type_info@@AAAAAV0@ABV0@@Z
1570 stub -noname ??0exception@std@@QAA@XZ
1571 stub -noname ??0exception@std@@QAA@PBD@Z
1572 stub -noname ??0exception@std@@QAA@ABV01@@Z
1573 stub -noname ??4exception@std@@QAAAAV01@ABV01@@Z
1574 stub -noname ??1exception@std@@UAA@XZ
1575 stub -noname ?what@exception@std@@UBAPBDXZ
1576 stub -noname ??_L@YAXPAXIHP6AX0@Z1@Z
1577 stub -noname ??_N@YAXPAXIHP6AX0@Z1@Z
1578 stub -noname ??_M@YAXPAXIHP6AX0@Z@Z
1579 stub -noname ??_7exception@std@@6B@
1580 stub -noname ??_7type_info@@6B@
1581 stub -noname GetSystemPowerState
1582 stdcall SetSystemPowerState(long long) kernel32.SetSystemPowerState
1583 stub -noname SetPowerRequirement
1584 stub -noname ReleasePowerRequirement
1585 stub -noname RequestPowerNotifications
1586 stub -noname StopPowerNotifications
1588 stub -noname DevicePowerNotify
1589 stub -noname mixerGetControlDetails
1590 stub -noname mixerGetID
1591 stub -noname mixerGetDevCaps
1592 stub -noname mixerGetLineControls
1593 stub -noname mixerGetLineInfo
1594 stub -noname mixerGetNumDevs
1595 stub -noname mixerOpen
1596 stub -noname mixerMessage
1597 stub -noname mixerSetControlDetails
1598 stub -noname mixerClose
1599 stdcall CryptProtectData(ptr wstr ptr ptr ptr long ptr) crypt32.CryptProtectData
1600 stdcall CryptUnprotectData(ptr ptr ptr ptr ptr long ptr) crypt32.CryptUnprotectData
1601 stub -noname CeGenRandom
1602 stub -noname MapCallerPtr
1603 stub -noname MapPtrToProcWithSize
1604 stub -noname RemoteHeapAlloc
1605 stub -noname RemoteHeapReAlloc
1606 stub -noname RemoteHeapFree
1607 stub -noname RemoteHeapSize
1608 cdecl setvbuf(ptr str long long) msvcrt.setvbuf
1609 stub -noname RegisterPowerRelationship
1610 stub -noname ReleasePowerRelationship
1611 stub -noname ChangeDisplaySettingsEx
1612 stub -noname ResourceCreateList
1613 stub -noname ResourceRequest
1614 stub -noname ResourceRelease
1615 stdcall InvalidateRgn(long long long) user32.InvalidateRgn
1616 stdcall ValidateRgn(long long) user32.ValidateRgn
1617 stdcall ExtCreateRegion(ptr long ptr) gdi32.ExtCreateRegion
1618 cdecl -arch=win32 ?_query_new_handler@@YAP6AHI@ZXZ() msvcrt.?_query_new_handler@@YAP6AHI@ZXZ
1619 cdecl ?set_new_handler@@YAP6AXXZP6AXXZ@Z(ptr) msvcrt.?set_new_handler@@YAP6AXXZP6AXXZ@Z
1621 cdecl -ret64 _abs64(int64) msvcrt._abs64
1622 stub -noname _byteswap_uint64
1623 stub -noname _byteswap_ulong
1624 stub -noname _byteswap_ushort
1625 stub -noname _CountLeadingOnes
1626 stub -noname _CountLeadingOnes64
1627 stub -noname _CountLeadingSigns
1628 stub -noname _CountLeadingSigns64
1629 stub -noname _CountLeadingZeros
1630 stub -noname _CountLeadingZeros64
1631 stub -noname _CountOneBits
1632 stub -noname _CountOneBits64
1633 stub -noname _isnanf
1634 stub -noname _isunordered
1635 stub -noname _isunorderedf
1636 stub -noname _MulHigh
1637 stub -noname _MulUnsignedHigh
1638 cdecl -ret64 _rotl64(int64 long) msvcrt._rotl64
1639 cdecl -ret64 _rotr64(int64 long) msvcrt._rotr64
1640 stub -noname ceilf(float)
1641 stub -noname fabsf
1642 stub -noname floorf(float)
1643 stub -noname fmodf(float float)
1644 stub -noname sqrtf(float)
1645 cdecl _XcptFilter(long ptr) msvcrt._XcptFilter
1646 stub -noname ??2@YAPAXIABUnothrow_t@std@@@Z
1647 stub -noname ?nothrow@std@@3Unothrow_t@1@B
1648 cdecl ?_set_new_mode@@YAHH@Z(long) msvcrt.?_set_new_mode@@YAHH@Z
1649 cdecl ?_query_new_mode@@YAHXZ() msvcrt.?_query_new_mode@@YAHXZ
1650 cdecl -arch=win32 ?_set_new_handler@@YAP6AHI@ZP6AHI@Z@Z(ptr) msvcrt.?_set_new_handler@@YAP6AHI@ZP6AHI@Z@Z
1651 stdcall MoveToEx(long long long ptr) gdi32.MoveToEx
1652 stdcall LineTo(long long long) gdi32.LineTo
1653 stdcall GetCurrentPositionEx(long ptr) gdi32.GetCurrentPositionEx
1654 stdcall SetTextAlign(long long) gdi32.SetTextAlign
1655 stdcall GetTextAlign(long) gdi32.GetTextAlign
1658 stub -noname ?_Xlen@std@@YAXXZ
1659 stub -noname ?_Xran@std@@YAXXZ
1660 stub -noname ?_Nomemory@std@@YAXXZ
1661 stub -noname ??_U@YAPAXIABUnothrow_t@std@@@Z
1662 stub -noname ??3@YAXPAXABUnothrow_t@std@@@Z
1663 stub -noname ??_V@YAXPAXABUnothrow_t@std@@@Z
1664 stub -noname GetCharWidth32
1665 stdcall GetDIBColorTable(long long long ptr) gdi32.GetDIBColorTable
1666 stdcall SetDIBColorTable(long long long ptr) gdi32.SetDIBColorTable
1667 stdcall StretchDIBits(long long long long long long long long long ptr ptr long long) gdi32.StretchDIBits
1668 stub -noname DDKReg_GetWindowInfo
1669 stub -noname DDKReg_GetIsrInfo
1670 stub -noname DDKReg_GetPciInfo
1671 stub -noname LoadKernelLibrary
1672 stdcall RedrawWindow(long ptr long long) user32.RedrawWindow
1673 stub -noname RasGetEapUserData
1674 stub -noname RasSetEapUserData
1675 stub -noname RasGetEapConnectionData
1676 stub -noname RasSetEapConnectionData
1677 stub -noname QueryInstructionSet
1678 stub -noname SetDevicePower
1679 stub -noname GetDevicePower
1680 stub -noname IsSystemFile
1681 stub -noname CeLogGetZones
1682 stdcall FindFirstChangeNotificationW(wstr long long) kernel32.FindFirstChangeNotificationW
1683 stdcall FindNextChangeNotification(long) kernel32.FindNextChangeNotification
1684 stdcall FindCloseChangeNotification(long) kernel32.FindCloseChangeNotification
1685 stub -noname AFS_FindFirstChangeNotificationW
1686 stub -noname GetUserDirectory
1687 stub -noname AdvertiseInterface
1688 stub -noname CeSetPowerOnEvent
1689 stub -noname StringCchCopyW
1690 stub -noname StringCbCopyW
1691 stub -noname StringCchCopyExW
1692 stub -noname StringCbCopyExW
1693 stub -noname StringCchCatW
1694 stub -noname StringCbCatW
1695 stub -noname StringCchCatExW
1696 stub -noname StringCbCatExW
1697 stub -noname StringCchVPrintfW
1698 stub -noname StringCbVPrintfW
1699 stub -noname StringCchPrintfW
1700 stub -noname StringCbPrintfW
1701 stub -noname StringCchPrintfExW
1702 stub -noname StringCbPrintfExW
1703 stub -noname StringCchVPrintfExW
1704 stub -noname StringCbVPrintfExW
1705 stub -noname StringCchCopyA
1706 stub -noname StringCbCopyA
1707 stub -noname StringCchCopyExA
1708 stub -noname StringCbCopyExA
1709 stub -noname StringCchCatA
1710 stub -noname StringCbCatA
1711 stub -noname StringCchCatExA
1712 stub -noname StringCbCatExA
1713 stub -noname StringCchVPrintfA
1714 stub -noname StringCbVPrintfA
1715 stub -noname StringCchPrintfA
1716 stub -noname StringCbPrintfA
1717 stub -noname StringCchPrintfExA
1718 stub -noname StringCbPrintfExA
1719 stub -noname StringCchVPrintfExA
1720 stub -noname StringCbVPrintfExA
1721 stub -noname GetModuleInformation
1722 stub -noname GwesPowerDown
1723 stub -noname GwesPowerUp
1724 stub -noname VirtualSetAttributes
1725 stdcall SetBitmapBits(long long ptr) gdi32.SetBitmapBits
1726 stdcall SetDIBitsToDevice(long long long long long long long long long ptr ptr long) gdi32.SetDIBitsToDevice
1727 stub -noname GetProcessIDFromIndex
1742 stub -noname StringCchCopyNW
1743 stub -noname StringCbCopyNW
1744 stub -noname StringCchCatNW
1745 stub -noname StringCbCatNW
1746 stub -noname StringCchCatNExW
1747 stub -noname StringCbCatNExW
1748 stub -noname StringCchLengthW
1749 stub -noname StringCbLengthW
1750 stub -noname StringCchCopyNA
1751 stub -noname StringCbCopyNA
1752 stub -noname StringCchCatNA
1753 stub -noname StringCbCatNA
1754 stub -noname StringCchCatNExA
1755 stub -noname StringCbCatNExA
1756 stub -noname StringCchLengthA
1757 stub -noname StringCbLengthA
1758 stdcall IsProcessorFeaturePresent(long) kernel32.IsProcessorFeaturePresent
1759 stub -noname ServiceClosePort
1760 stub -noname GetCallStackSnapshot
1761 stub -noname Int_CreateEventW
1762 stub -noname Int_CloseHandle
1763 stub -noname GradientFill
1764 stub -noname PowerPolicyNotify
1765 stub -noname CacheRangeFlush
1766 stdcall ActivateKeyboardLayout(long long) user32.ActivateKeyboardLayout
1767 stdcall GetKeyboardLayoutList(long ptr) user32.GetKeyboardLayoutList
1768 stdcall LoadKeyboardLayoutW(wstr long) user32.LoadKeyboardLayoutW
1769 stub -noname ImmGetKeyboardLayout
1770 stdcall InvertRect(long ptr) user32.InvertRect
1771 stdcall GetKeyboardType(long) user32.GetKeyboardType
1775 stub -noname CeSetProcessVersion
1776 stub -noname DecompressBinaryBlock
1777 stub -noname EnumDisplaySettings
1778 stub -noname EnumDisplayDevices
1779 stub -noname GetCharABCWidths
1780 stub -noname PageOutModule
1781 stub -noname CeZeroPointer
1789 stub -noname A_SHAInit
1790 stub -noname A_SHAUpdate
1791 stub -noname A_SHAFinal
1792 stub -noname MD5Init
1793 stub -noname MD5Update
1794 stub -noname MD5Final
1795 stub -noname SetUserDefaultLCID
1796 stub -noname UpdateNLSInfoEx
1797 stub -noname InterruptMask
1798 stub -noname CeGetFileNotificationInfo
1799 stub -noname ReinitLocale
1800 stub -noname ProfileCaptureStatus
1801 stub -noname ProfileStartEx
1802 stub -noname GetForegroundKeyboardLayoutHandle
1810 stub -noname ShowStartupWindow
1811 stub -noname GetThreadCallStack
1812 stub -noname CeVirtualSharedAlloc
1813 stub -noname waveOutGetProperty
1814 stub -noname waveOutSetProperty
1815 stub -noname waveInGetProperty
1816 stub -noname waveInSetProperty
1818 stub -noname ClearEventLogW
1819 stub -noname ReportEventW
1820 stub -noname RegisterEventSourceW
1821 stub -noname DeregisterEventSource
1822 stdcall GetIconInfo(long ptr) user32.GetIconInfo
1823 stub -noname CeGetNotificationThreadId
1824 stdcall GetStretchBltMode(long) gdi32.GetStretchBltMode
1825 stdcall SetStretchBltMode(long long) gdi32.SetStretchBltMode
1826 stub -noname DeleteStaticMapping
1827 stub -noname VerifyUser
1828 stub -noname LASSReloadConfig
1829 stub -noname ForcePixelDoubling
1830 stub -noname IsForcePixelDoubling
1831 stdcall ReadFileScatter(long ptr long ptr ptr) kernel32.ReadFileScatter
1832 stdcall WriteFileGather(long ptr long ptr ptr) kernel32.WriteFileGather
1833 stub -noname ResourceRequestEx
1834 stub -noname ResourceMarkAsShareable
1835 stub -noname ResourceDestroyList
1836 stub -noname CeHeapCreate
1837 stdcall -ordinal DPA_Create(long) comctl32.DPA_Create
1838 stdcall -ordinal DPA_CreateEx(long long) comctl32.DPA_CreateEx
1839 stdcall -ordinal DPA_Clone(ptr ptr) comctl32.DPA_Clone
1840 stdcall -ordinal DPA_DeleteAllPtrs(ptr) comctl32.DPA_DeleteAllPtrs
1841 stdcall -ordinal DPA_DeletePtr(ptr long) comctl32.DPA_DeletePtr
1842 stdcall -ordinal DPA_Destroy(ptr) comctl32.DPA_Destroy
1843 stdcall -ordinal DPA_DestroyCallback(ptr ptr long) comctl32.DPA_DestroyCallback
1844 stdcall -ordinal DPA_EnumCallback(long long long) comctl32.DPA_EnumCallback
1845 stdcall -ordinal DPA_GetPtr(ptr long) comctl32.DPA_GetPtr
1846 stdcall -ordinal DPA_GetPtrIndex(ptr ptr) comctl32.DPA_GetPtrIndex
1847 stdcall -ordinal DPA_Grow(ptr long) comctl32.DPA_Grow
1848 stdcall -ordinal DPA_InsertPtr(ptr long ptr) comctl32.DPA_InsertPtr
1849 stdcall -ordinal DPA_Search(ptr ptr long ptr long long) comctl32.DPA_Search
1850 stdcall -ordinal DPA_SetPtr(ptr long ptr) comctl32.DPA_SetPtr
1851 stdcall -ordinal DPA_Sort(ptr ptr long) comctl32.DPA_Sort
1852 stdcall -ordinal DSA_Create(long long) comctl32.DSA_Create
1853 stub -noname DSA_Clone
1854 stdcall -ordinal DSA_DeleteAllItems(ptr) comctl32.DSA_DeleteAllItems
1855 stdcall -ordinal DSA_DeleteItem(ptr long) comctl32.DSA_DeleteItem
1856 stdcall -ordinal DSA_Destroy(ptr) comctl32.DSA_Destroy
1857 stdcall -ordinal DSA_DestroyCallback(ptr ptr long) comctl32.DSA_DestroyCallback
1858 stdcall -ordinal DSA_EnumCallback(ptr ptr long) comctl32.DSA_EnumCallback
1859 stdcall -ordinal DSA_GetItem(ptr long long) comctl32.DSA_GetItem
1860 stdcall -ordinal DSA_GetItemPtr(ptr long) comctl32.DSA_GetItemPtr
1861 stub -noname DSA_Grow
1862 stdcall -ordinal DSA_InsertItem(ptr long long) comctl32.DSA_InsertItem
1863 stub -noname DSA_Search
1864 stdcall -ordinal DSA_SetItem(ptr long long) comctl32.DSA_SetItem
1865 stub -noname DSA_SetRange
1866 stub -noname DSA_Sort
1867 stub -noname GetGweApiSetTables
1868 stub -noname StringCchCopyNExW
1869 stub -noname StringCbCopyNExW
1870 stub -noname GetDeviceInformationByDeviceHandle
1871 stub -noname GetDeviceInformationByFileHandle
1872 stub -noname FindFirstDevice
1873 stub -noname FindNextDevice
1874 stub -noname EnumDeviceInterfaces
1875 stub -noname __security_gen_cookie
1876 stub -noname __report_gsfailure
1877 stdcall MulDiv(long long long) kernel32.MulDiv
1878 stub -noname CredWrite
1879 stub -noname CredRead
1880 stub -noname CredUpdate
1881 stub -noname CredDelete
1882 stub -noname CredFree
1883 stub -noname AlphaBlend
1884 stdcall HeapCompact(long long) kernel32.HeapCompact
1885 stdcall EnumFontFamiliesExW(long ptr ptr long long) gdi32.EnumFontFamiliesExW
1886 stub -noname GetNlsTables
1887 stdcall GetCharABCWidthsI(long long long ptr ptr) gdi32.GetCharABCWidthsI
1888 stdcall GetFontData(long long long ptr long) gdi32.GetFontData
1889 stdcall GetOutlineTextMetricsW(long long ptr) gdi32.GetOutlineTextMetricsW
1890 stdcall SetLayout(long long) gdi32.SetLayout
1891 stdcall GetLayout(long) gdi32.GetLayout
1892 stub -noname CeAddDatabaseProps
1893 stub -noname CeAddSyncPartner
1894 stub -noname CeAttachCustomTrackingData
1895 stub -noname CeBeginSyncSession
1896 stub -noname CeBeginTransaction
1897 stub -noname CeCreateDatabaseWithProps
1898 stub -noname CeCreateSession
1899 stub -noname CeEndSyncSession
1900 stub -noname CeEndTransaction
1901 stub -noname CeFindNextChangedRecord
1902 stub -noname CeGetChangedRecordCnt
1903 stub -noname CeGetChangedRecords
1904 stub -noname CeGetCustomTrackingData
1905 stub -noname CeGetDatabaseProps
1906 stub -noname CeGetDatabaseSession
1907 stub -noname CeGetPropChangeInfo
1908 stub -noname CeGetRecordChangeInfo
1909 stub -noname CeMarkRecord
1910 stub -noname CeMountDBVolEx
1911 stub -noname CeOpenDatabaseInSession
1912 stub -noname CeOpenStream
1913 stub -noname CePurgeTrackingData
1914 stub -noname CePurgeTrackingGenerations
1915 stub -noname CeRemoveDatabaseProps
1916 stub -noname CeRemoveDatabaseTracking
1917 stub -noname CeRemoveSyncPartner
1918 stub -noname CeSetSessionOption
1919 stub -noname CeStreamRead
1920 stub -noname CeStreamSaveChanges
1921 stub -noname CeStreamSeek
1922 stub -noname CeStreamSetSize
1923 stub -noname CeStreamWrite
1924 stub -noname CeTrackDatabase
1925 stub -noname CeTrackProperty
1950 stub -noname CeFindFirstRegChange
1951 stub -noname CeFindNextRegChange
1952 stub -noname CeFindCloseRegChange
1953 stub -noname ConnectHdstub
1954 stub -noname ConnectOsAxsT0
1955 stub -noname AttachHdstub
1956 stub -noname AttachOsAxsT0
1957 stub -noname CeGetCanonicalPathNameW
1958 stdcall CopyFileExW(wstr wstr ptr ptr ptr long) kernel32.CopyFileExW
1959 stub -noname MatchesWildcardMask
1960 stub -noname CaptureDumpFileOnDevice
1961 stub -noname GetDeviceHandleFromContext
1962 stdcall SetTextCharacterExtra(long long) gdi32.SetTextCharacterExtra
1963 stdcall GetTextCharacterExtra(long) gdi32.GetTextCharacterExtra
1964 stub -noname ReportFault
1965 stub -noname CeFsIoControlW
1966 stub -noname AFS_FsIoControlW
1967 cdecl ?uncaught_exception@std@@YA_NXZ() msvcp71.?uncaught_exception@std@@YA_NXZ
1968 stdcall LockFileEx(long long long long long ptr) kernel32.LockFileEx
1969 stdcall UnlockFileEx(long long long long ptr) kernel32.UnlockFileEx
1970 stub -noname OpenEventLogW
1971 stub -noname CloseEventLog
1972 stub -noname BackupEventLogW
1973 stub -noname LockEventLog
1974 stub -noname UnLockEventLog
1975 stub -noname ReadEventLogRaw
1976 stub -noname CreateEnrollmentConfigDialog
1977 stdcall -ordinal SHLoadIndirectString(wstr ptr long ptr) shlwapi.SHLoadIndirectString
1978 stub -noname CeGetVolumeInfoW
1979 stub -noname ImmActivateLayout
1980 stub -noname ImmSendNotification
1981 stub -noname IsNamedEventSignaled
1982 stub -noname AttachOsAxsT1
1983 stub -noname ConnectOsAxsT1
1984 stdcall SetWindowOrgEx(long long long ptr) gdi32.SetWindowOrgEx
1985 stdcall GetWindowOrgEx(long ptr) gdi32.GetWindowOrgEx
1986 stdcall GetWindowExtEx(long ptr) gdi32.GetWindowExtEx
1987 stdcall OffsetViewportOrgEx(long long long ptr) gdi32.OffsetViewportOrgEx
1988 stdcall GetViewportOrgEx(long ptr) gdi32.GetViewportOrgEx
1989 stdcall GetViewportExtEx(long ptr) gdi32.GetViewportExtEx
1990 stdcall GetROP2(long) gdi32.GetROP2
1991 stdcall DebugActiveProcessStop(long) kernel32.DebugActiveProcessStop
1992 stdcall DebugSetProcessKillOnExit(long) kernel32.DebugSetProcessKillOnExit
1993 stub -noname GetDeviceUniqueID
1994 stub -noname CeGetModuleInfo
1995 stub -noname RequestBluetoothNotifications
1996 stub -noname StopBluetoothNotifications
1997 stub -noname Int_HeapCreate
1998 stub -noname Int_HeapDestroy
1999 stub -noname HeapAllocTrace
2000 stub -noname __rt_sdiv64by64
2001 stub -noname __rt_srem64by64
2002 stub -noname __rt_udiv64by64
2003 stub -noname __rt_urem64by64
2005 stub -noname __rt_sdiv
2006 stub -noname __rt_sdiv10
2008 stub -noname __rt_udiv
2009 stub -noname __rt_udiv10
2010 stub -noname __rt_srsh
2011 stub -noname __rt_ursh
2012 stub -noname __utod
2013 stub -noname __u64tos
2014 stub -noname __u64tod
2015 stub -noname __subs
2016 stub -noname __subd
2017 stub -noname __stou64
2018 stub -noname __stou
2019 stub -noname __stoi64
2020 stub -noname __stoi
2021 stub -noname __stod
2022 stub -noname __nes
2023 stub -noname __negs
2024 stub -noname __negd
2025 stub -noname __ned
2026 stub -noname __muls
2027 stub -noname __muld
2028 stub -noname __lts
2029 stub -noname __ltd
2030 stub -noname __les
2031 stub -noname __led
2032 stub -noname __itos
2033 stub -noname __itod
2034 stub -noname __i64tos
2035 stub -noname __i64tod
2036 stub -noname __gts
2037 stub -noname __gtd
2038 stub -noname __ges
2039 stub -noname __ged
2040 stub -noname __eqs
2041 stub -noname __eqd
2042 stub -noname __dtou64
2043 stub -noname __dtou
2044 stub -noname __dtos
2045 stub -noname __dtoi64
2046 stub -noname __dtoi
2047 stub -noname __divs
2048 stub -noname __divd
2049 stub -noname __cmps
2050 stub -noname __cmpd
2051 stub -noname __adds
2052 stub -noname __utos
2053 stub -noname __addd
2054 stub -noname -norelay -private setjmp(ptr)
2055 stub -noname _mbmemset
2500 stub -noname Int_HeapAlloc
2501 stub -noname Int_HeapReAlloc
2502 stub -noname Int_HeapSize
2503 stub -noname Int_HeapFree
2504 stub -noname CeRegTestSetValueW
2505 stub -noname CeRegGetInfo
2506 stub -noname CeRegGetNotificationInfo
2507 stdcall CheckRemoteDebuggerPresent(long ptr) kernel32.CheckRemoteDebuggerPresent
2508 stub -noname CeSafeCopyMemory
2509 stub -noname CeCertVerify
2510 stub -noname CeGetProcessTrust
2511 stub -noname CeOpenFileHandle

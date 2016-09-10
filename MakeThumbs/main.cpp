/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <Windows.h>
#include <tchar.h>
#include <Thumbcache.h>
#include <ShlObj.h>
#include <strsafe.h>

bool MakeThumbnails(const TCHAR *path, IThumbnailCache *thumbCache,
                    UINT thumbSize)
{
	bool rc = false;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	size_t path_len;
	StringCchLength(path, MAX_PATH, &path_len);

	if (path_len > MAX_PATH - 3)
	{
		_tprintf(TEXT("Directory path is too long.\n"));
		goto cleanup;
	}

	TCHAR findPattern[MAX_PATH];
	StringCchCopy(findPattern, MAX_PATH, path);
	StringCchCat(findPattern, MAX_PATH, TEXT("\\*"));

	WIN32_FIND_DATA ffd;
	hFind = FindFirstFile(findPattern, &ffd);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		_tprintf(TEXT("Enumerating files in directory failed\n"));
		goto cleanup;
	}

	do
	{
		if (_tcscmp(ffd.cFileName, TEXT(".") ) == 0 ||
		    _tcscmp(ffd.cFileName, TEXT("..")) == 0)
		{
			continue;
		}

		TCHAR fullPath[MAX_PATH];
		StringCchCopy(fullPath, MAX_PATH, path);
		StringCchCat(fullPath, MAX_PATH, TEXT("\\"));

		if (!SUCCEEDED(StringCchCat(fullPath, MAX_PATH, ffd.cFileName)))
		{
			_tprintf(TEXT("Path of item \"%s\\%s\" is too long!\n"), path,
			         ffd.cFileName);
			goto cleanup;
		}

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!MakeThumbnails(fullPath, thumbCache, thumbSize))
				goto cleanup;
		}
		else
		{
			_tprintf(TEXT("Generating thumbnail for %s\n"), fullPath);

			PIDLIST_ABSOLUTE pidList;
			if (!SUCCEEDED(SHParseDisplayName(fullPath, nullptr, &pidList, SFGAO_FOLDER,
			                                  nullptr)))
			{
				_tprintf(TEXT("Path of \"%s\" couldn't be parsed!\n"), fullPath);
				goto cleanup;
			}

			IShellItem *shellItem;
			if (!SUCCEEDED(SHCreateShellItem(NULL, NULL, pidList, &shellItem)))
			{
				_tprintf(TEXT("Shell item for \"%s\" couldn't be created!\n"), fullPath);
				ILFree(pidList);
				goto cleanup;
			}
			ILFree(pidList);

			thumbCache->GetThumbnail(shellItem, thumbSize, WTS_EXTRACT, nullptr,
			                         nullptr, nullptr);
			shellItem->Release();
		}
	}
	while (FindNextFile(hFind, &ffd) != 0);

	if (GetLastError() != ERROR_NO_MORE_FILES)
	{
		_tprintf(TEXT("Enumerating files in directory failed\n"));
		goto cleanup;
	}

	rc = true;
cleanup:
	if (hFind != INVALID_HANDLE_VALUE)
		FindClose(hFind);

	return rc;
}

int _tmain(int argc, TCHAR **argv) {
	bool rc = false;

	if (argc != 3)
	{
		_tprintf(TEXT("Usage: %s <directory name> <thumbnail size>\n"), argv[0]);
		return 1;
	}

	int thumbSize = _ttoi(argv[2]);
	if (thumbSize < 1 ||thumbSize > 1024)
	{
		_tprintf(TEXT("Invalid thumbnail size. It must be between 1 and 1024\n"));
		return 1;
	}

	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	IThumbnailCache *thumbCache;
	HRESULT hr = CoCreateInstance(CLSID_LocalThumbnailCache, nullptr,
	                              CLSCTX_INPROC, IID_IThumbnailCache,
	                              reinterpret_cast<void**>(&thumbCache));
	if (!SUCCEEDED(hr))
	{
		_tprintf(TEXT("Accessing thumbnail cache failed\n"));
		return 1;
	}

	if (!MakeThumbnails(argv[1], thumbCache, thumbSize))
		goto cleanup;

	rc = true;
cleanup:
	if (thumbCache != nullptr)
		thumbCache->Release();

	CoUninitialize();
	return rc ? 0 : 1;
}

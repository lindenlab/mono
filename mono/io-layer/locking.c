/*
 * io.c:  File, console and find handles
 *
 * Author:
 *	Dick Porter (dick@ximian.com)
 *
 * (C) 2002 Ximian, Inc.
 * Copyright (c) 2002-2009 Novell, Inc.
 * Licensed under the MIT license. See LICENSE file in the project root for full license information.
 */
#include <config.h>
#include <stdio.h>
#include <glib.h>
#include <fcntl.h>
#include <errno.h>
#include <mono/io-layer/wapi.h>
#include <mono/io-layer/wapi-private.h>
#include <mono/io-layer/io-private.h>
#include <mono/io-layer/io-trace.h>
#include <mono/utils/mono-logger-internals.h>
#include <mono/utils/w32handle.h>

gboolean
_wapi_lock_file_region (int fd, off_t offset, off_t length)
{
#if defined(__native_client__)
	printf("WARNING: locking.c: _wapi_lock_file_region(): fcntl() not available on Native Client!\n");
	// behave as below -- locks are not available
	return(TRUE);
#else
	struct flock lock_data;
	int ret;

	if (offset < 0 || length < 0) {
		SetLastError (ERROR_INVALID_PARAMETER);
		return(FALSE);
	}

	lock_data.l_type = F_WRLCK;
	lock_data.l_whence = SEEK_SET;
	lock_data.l_start = offset;
	lock_data.l_len = length;
	
	do {
		ret = fcntl (fd, F_SETLK, &lock_data);
	} while(ret == -1 && errno == EINTR);
	
	MONO_TRACE (G_LOG_LEVEL_DEBUG, MONO_TRACE_IO_LAYER, "%s: fcntl returns %d", __func__, ret);

	if (ret == -1) {
		/*
		 * if locks are not available (NFS for example),
		 * ignore the error
		 */
		if (errno == ENOLCK
#ifdef EOPNOTSUPP
		    || errno == EOPNOTSUPP
#endif
#ifdef ENOTSUP
		    || errno == ENOTSUP
#endif
		   ) {
			return (TRUE);
		}
		
		SetLastError (ERROR_LOCK_VIOLATION);
		return(FALSE);
	}

	return(TRUE);
#endif /* __native_client__ */
}

gboolean
_wapi_unlock_file_region (int fd, off_t offset, off_t length)
{
#if defined(__native_client__)
	printf("WARNING: locking.c: _wapi_unlock_file_region(): fcntl() not available on Native Client!\n");
	return (TRUE);
#else
	struct flock lock_data;
	int ret;

	lock_data.l_type = F_UNLCK;
	lock_data.l_whence = SEEK_SET;
	lock_data.l_start = offset;
	lock_data.l_len = length;
	
	do {
		ret = fcntl (fd, F_SETLK, &lock_data);
	} while(ret == -1 && errno == EINTR);
	
	MONO_TRACE (G_LOG_LEVEL_DEBUG, MONO_TRACE_IO_LAYER, "%s: fcntl returns %d", __func__, ret);
	
	if (ret == -1) {
		/*
		 * if locks are not available (NFS for example),
		 * ignore the error
		 */
		if (errno == ENOLCK
#ifdef EOPNOTSUPP
		    || errno == EOPNOTSUPP
#endif
#ifdef ENOTSUP
		    || errno == ENOTSUP
#endif
		   ) {
			return (TRUE);
		}
		
		SetLastError (ERROR_LOCK_VIOLATION);
		return(FALSE);
	}

	return(TRUE);
#endif /* __native_client__ */
}

gboolean
LockFile (gpointer handle, guint32 offset_low, guint32 offset_high,
	  guint32 length_low, guint32 length_high)
{
	struct _WapiHandle_file *file_handle;
	gboolean ok;
	off_t offset, length;
	int fd = GPOINTER_TO_UINT(handle);
	
	ok = mono_w32handle_lookup (handle, MONO_W32HANDLE_FILE,
				  (gpointer *)&file_handle);
	if (ok == FALSE) {
		g_warning ("%s: error looking up file handle %p", __func__,
			   handle);
		SetLastError (ERROR_INVALID_HANDLE);
		return(FALSE);
	}

	if (!(file_handle->fileaccess & GENERIC_READ) &&
	    !(file_handle->fileaccess & GENERIC_WRITE) &&
	    !(file_handle->fileaccess & GENERIC_ALL)) {
		MONO_TRACE (G_LOG_LEVEL_DEBUG, MONO_TRACE_IO_LAYER, "%s: handle %p doesn't have GENERIC_READ or GENERIC_WRITE access: %u", __func__, handle, file_handle->fileaccess);
		SetLastError (ERROR_ACCESS_DENIED);
		return(FALSE);
	}

#ifdef HAVE_LARGE_FILE_SUPPORT
	offset = ((gint64)offset_high << 32) | offset_low;
	length = ((gint64)length_high << 32) | length_low;

	MONO_TRACE (G_LOG_LEVEL_DEBUG, MONO_TRACE_IO_LAYER, "%s: Locking handle %p, offset %lld, length %lld", __func__, handle, offset, length);
#else
	if (offset_high > 0 || length_high > 0) {
		SetLastError (ERROR_INVALID_PARAMETER);
		return (FALSE);
	}
	offset = offset_low;
	length = length_low;

	MONO_TRACE (G_LOG_LEVEL_DEBUG, MONO_TRACE_IO_LAYER, "%s: Locking handle %p, offset %ld, length %ld", __func__,
		   handle, offset, length);
#endif

	return(_wapi_lock_file_region (fd, offset, length));
}

gboolean
UnlockFile (gpointer handle, guint32 offset_low,
	    guint32 offset_high, guint32 length_low,
	    guint32 length_high)
{
	struct _WapiHandle_file *file_handle;
	gboolean ok;
	off_t offset, length;
	int fd = GPOINTER_TO_UINT(handle);
	
	ok = mono_w32handle_lookup (handle, MONO_W32HANDLE_FILE,
				  (gpointer *)&file_handle);
	if (ok == FALSE) {
		g_warning ("%s: error looking up file handle %p", __func__,
			   handle);
		SetLastError (ERROR_INVALID_HANDLE);
		return(FALSE);
	}
	
	if (!(file_handle->fileaccess & GENERIC_READ) &&
	    !(file_handle->fileaccess & GENERIC_WRITE) &&
	    !(file_handle->fileaccess & GENERIC_ALL)) {
		MONO_TRACE (G_LOG_LEVEL_DEBUG, MONO_TRACE_IO_LAYER, "%s: handle %p doesn't have GENERIC_READ or GENERIC_WRITE access: %u", __func__, handle, file_handle->fileaccess);
		SetLastError (ERROR_ACCESS_DENIED);
		return(FALSE);
	}

#ifdef HAVE_LARGE_FILE_SUPPORT
	offset = ((gint64)offset_high << 32) | offset_low;
	length = ((gint64)length_high << 32) | length_low;

	MONO_TRACE (G_LOG_LEVEL_DEBUG, MONO_TRACE_IO_LAYER, "%s: Unlocking handle %p, offset %lld, length %lld", __func__, handle, offset, length);
#else
	offset = offset_low;
	length = length_low;

	MONO_TRACE (G_LOG_LEVEL_DEBUG, MONO_TRACE_IO_LAYER, "%s: Unlocking handle %p, offset %ld, length %ld", __func__, handle, offset, length);
#endif

	return(_wapi_unlock_file_region (fd, offset, length));
}

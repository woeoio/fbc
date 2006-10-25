/*
 *  libfb - FreeBASIC's runtime library
 *	Copyright (C) 2004-2007 The FreeBASIC development team.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  As a special exception, the copyright holders of this library give
 *  you permission to link this library with independent modules to
 *  produce an executable, regardless of the license terms of these
 *  independent modules, and to copy and distribute the resulting
 *  executable under terms of your choice, provided that you also meet,
 *  for each linked independent module, the terms and conditions of the
 *  license of that module. An independent module is a module which is
 *  not derived from or based on this library. If you modify this library,
 *  you may extend this exception to your version of the library, but
 *  you are not obligated to do so. If you do not wish to do so, delete
 *  this exception statement from your version.
 */

/*
 * libfb_file_dir.c -- dir$ implementation
 *
 * chng: jan/2005 written [lillo]
 *
 */

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "fb.h"


/*:::::*/
static void close_dir ( void )
{
	FB_DIRCTX *ctx = FB_TLSGETCTX( DIR );

	closedir( ctx->dir );
	ctx->in_use = FALSE;
}


/*:::::*/
static int get_attrib ( char *name, struct stat *info )
{
	int attrib = 0, mask;

	/* read only */
	if( info->st_uid == geteuid() )
		mask = S_IWUSR;
	else if( info->st_gid == getegid() ) 
		mask = S_IWGRP;
	else
		mask = S_IWOTH;

    if( (info->st_mode & mask) == 0 )
		attrib |= 0x1;

	if( name[0] == '.' )
		attrib |= 0x2;	/* hidden */

	if( S_ISCHR( info->st_mode ) || S_ISBLK( info->st_mode ) || S_ISFIFO( info->st_mode ) || S_ISSOCK( info->st_mode ) )
		attrib |= 0x4;	/* system */

	if( S_ISDIR( info->st_mode ) )
		attrib |= 0x10;	/* directory */
	else
		attrib |= 0x20;	/* archive */

	return attrib;
}


/*:::::*/
static int match_spec( char *name )
{
	FB_DIRCTX *ctx = FB_TLSGETCTX( DIR );
	char *any = NULL;
	char *spec;

	spec = ctx->filespec;

	while( ( *spec ) || ( *name ) )
	{
		switch( *spec )
		{
			case '*':
				any = spec;
				spec++;
				while( ( *name != *spec ) && ( *name ) )
					name++;
				break;

			case '?':
				spec++;
				if( *name )
					name++;
				break;

			default:
				if( *spec != *name )
				{
					if( ( any ) && ( *name ) )
						spec = any;
					else
						return FALSE;
				}
				else
				{
					spec++;
					name++;
				}
				break;
		}
	}

	return TRUE;
}

/*:::::*/
static char *find_next ( int *attrib )
{
	FB_DIRCTX *ctx = FB_TLSGETCTX( DIR );
	char *name = NULL;
	struct stat	info;
	struct dirent *entry;
	char buffer[MAX_PATH];

	do
	{
		entry = readdir( ctx->dir );
		if( !entry )
		{
			close_dir( );
			return NULL;
		}
		name = entry->d_name;
		strcpy( buffer, ctx->dirname );
		strncat( buffer, name, MAX_PATH );
		buffer[MAX_PATH-1] = '\0';
		
		if( stat( buffer, &info ) )
			continue;
		
		*attrib = get_attrib( name, &info );
	}
	while( ( *attrib & ~ctx->attrib ) || !match_spec( name ) );

	return name;
}


/*:::::*/
FBCALL FBSTRING *fb_Dir ( FBSTRING *filespec, int attrib, int *out_attrib )
{
	FB_DIRCTX *ctx;
	FBSTRING *res;
	int len, tmp_attrib;
	char *name, *p;
	struct stat	info;
	
	if( out_attrib == NULL ) 
		out_attrib = &tmp_attrib;

	len = FB_STRSIZE( filespec );
	name = NULL;

	ctx = FB_TLSGETCTX( DIR );

	if( len > 0 )
	{
		/* findfirst */

		if( ctx->in_use )
			close_dir( );

		if( strchr( filespec->data, '*' ) || strchr( filespec->data, '?' ) )
		{
			/* we have a pattern */

			p = strrchr( filespec->data, '/' );
			if( p )
			{
				strncpy( ctx->filespec, p + 1, MAX_PATH );
				ctx->filespec[MAX_PATH-1] = '\0';
				len = (p - filespec->data) + 1;
				if( len > MAX_PATH - 1 )
					len = MAX_PATH - 1;
				memcpy( ctx->dirname, filespec->data, len );
				ctx->dirname[len] = '\0';
			}
			else
			{
				strncpy( ctx->filespec, filespec->data, MAX_PATH );
				ctx->filespec[MAX_PATH-1] = '\0';
				strcpy( ctx->dirname, "./");
			}

			/* compatibility convertions */
			if( (!strcmp( ctx->filespec, "*.*" )) || (!strcmp( ctx->filespec, "*." )) )
				strcpy( ctx->filespec, "*" );

			if( (attrib & 0x10) == 0 )
				attrib |= 0x20;
			ctx->attrib = attrib;
			ctx->dir = opendir( ctx->dirname );
			if( ctx->dir )
			{
				name = find_next( out_attrib );
				if( name )
					ctx->in_use = TRUE;
			}
		}
		else
		{
			/* no pattern, use stat on single file */
			if( !stat( filespec->data, &info ) )
			{
				tmp_attrib = get_attrib( filespec->data, &info );
				if( (tmp_attrib & ~attrib ) == 0 )
				{
					name = strrchr( filespec->data, '/' );
					if( !name )
						name = filespec->data;
					else
						name++;
					*out_attrib = tmp_attrib;
				}
			}
		}
	}
	else 
	{
		/* findnext */
		if( ctx->in_use )
			name = find_next( out_attrib );
	}

	FB_STRLOCK();

	/* store filename if found */
	if( name )
	{
        len = strlen( name );
        res = fb_hStrAllocTemp_NoLock( NULL, len );
		if( res )
		{
			fb_hStrCopy( res->data, name, len );

		}
		else
			res = &__fb_ctx.null_desc;
	}
	else
	{
		res = &__fb_ctx.null_desc;
		*out_attrib = 0;
	}

	fb_hStrDelTemp_NoLock( filespec );

	FB_STRUNLOCK();

	return res;
}

/*:::::*/
FBCALL FBSTRING *fb_DirNext ( int *attrib )
{
	static FBSTRING fname = { 0 };
	return fb_Dir( &fname, 0, attrib );
}

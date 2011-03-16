#!/bin/sh
# Run this to generate all the initial makefiles, etc.

if test "z$1" = "z--clean"; then
    echo "Cleaning"

    rm -Rf autom4te.cache
    rm -f missing install-sh mkinstalldirs libtool ltmain.sh
    exit 1;
fi

requote()
{
    out=""
    for param in "$@" ; do
	p=`echo "$param" | sed "s/'/'\\\\\\''/g"`
	if test -z "$out" ; then
	    out="'$p'"
	else
	    out="$out '$p'"
	fi
    done
    echo "$out"
}

old_args=""
if test $# -eq 0 && test -f autogen.lastrun; then
    old_args=$(cat autogen.lastrun)
    echo "re-using arguments from last configure: $old_args";
fi

touch ChangeLog

if test "z$ACLOCAL_FLAGS" = "z" -a "z`uname -s`" = "zDarwin" ; then
    ACLOCAL_FLAGS="-I ./m4/mac"
fi
if test "z`uname -s`" != "zDarwin" ; then
    AUTOMAKE_EXTRA_FLAGS=--warnings=no-portability
fi

conf_args=$(requote "$@")

rm -f configure config.status config.h
autoreconf -i -f
aclocal $ACLOCAL_FLAGS || exit 1;
intltoolize --copy --force --automake || exit 1;
if test "x$NOCONFIGURE" = "x"; then
    if test -n "$old_args" ; then
        eval `echo ./configure $old_args`
    else
        echo "$(requote "$@")" > autogen.lastrun
        echo "./configure ${conf_args}"
        eval `echo ./configure ${conf_args}`
    fi
else
    echo "Skipping configure process."
fi

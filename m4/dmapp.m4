AC_DEFUN([CLAMPI_WITH_DMAPP],
    [AC_ARG_WITH(dmapp, AC_HELP_STRING([--with-dmapp], [compile with DMAPP header (needed by foMPI)]))
    dmapp_found=no
	AC_SUBST([FOMPI_CFLAGS], [])
	AC_SUBST([FOMPI_LDFLAGS], [])

    if test x"${with_dmapp}" == xyes; then
        AC_CHECK_HEADER(dmapp.h, dmapp_found=yes, [AC_MSG_ERROR([DMAPP support selected but headers not available!])])
    elif test x"${with_dmapp}" == xno; then
		dmapp_found=no
	elif test x"${with_dmapp}" != x; then
        CPPFLAGS="$CPPFLAGS -I${with_dmapp}"
        AC_CHECK_HEADER(dmapp.h, [dmapp_found=yes], [AC_MSG_ERROR([Can't find the DMAPP header files in ${with_dmapp}])])
    fi

    if test x"${dmapp_found}" == xyes; then
        AC_MSG_NOTICE([DMAPP support enabled])
        if test x${clampi_dmapp_path} != x; then
            CXXFLAGS="${CXXFLAGS} -I${with_dmapp}/"
        fi
    fi
    ]
)


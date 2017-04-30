AC_DEFUN([CLAMPI_WITH_FOMPI],
    [AC_ARG_WITH(fompi, AC_HELP_STRING([--with-fompi], [compile with foMPI]))
    fompi_found=no
	AC_SUBST([FOMPI_CFLAGS], [])
	AC_SUBST([FOMPI_LDFLAGS], [])

    if test x"${with_fompi}" == xyes; then
        AC_CHECK_HEADER(fompi.h, fompi_found=yes, [AC_MSG_ERROR([foMPI support selected but headers not available!])])
    elif test x"${with_fompi}" == xno; then
		fompi_found=no
	elif test x"${with_fompi}" != x; then
        CPPFLAGS="$CPPFLAGS -I${with_fompi}"
        AC_CHECK_HEADER(fompi.h, [clampi_fompi_path=${with_fompi}; fompi_found=yes], [AC_MSG_ERROR([Can't find the foMPI header files in ${with_fompi}])])
    fi

    if test x"${fompi_found}" == xyes; then
        AC_DEFINE(USE_FOMPI, 1, enables foMPI)
        AC_MSG_NOTICE([foMPI support enabled])
		AC_SUBST([FOMPI_CFLAGS], [])
		AC_SUBST([FOMPI_LDFLAGS], [])
        if test x${clampi_fompi_path} != x; then
            CXXFLAGS="${CXXFLAGS} -I${with_fompi}/"
            LDFLAGS="${LDFLAGS} -L${clampi_fompi_path}"
			AC_SUBST([FOMPI_CFLAGS], [-I${clampi_fompi_path}/])
			AC_SUBST([FOMPI_LDFLAGS], [-L${clampi_fompi_path}])
        fi
    fi
    ]
)


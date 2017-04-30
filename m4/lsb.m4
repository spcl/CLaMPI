AC_DEFUN([CLAMPI_WITH_LIBLSB],
    [AC_ARG_WITH(liblsb, AC_HELP_STRING([--with-liblsb], [compile with libLSB]))
    liblsb_found=no

    if test x"${with_liblsb}" == xyes; then
        AC_CHECK_HEADER(liblsb.h, liblsb_found=yes, [AC_MSG_ERROR([libLSB support selected but headers not available!])])
    elif test x"${with_liblsb}" == xno; then
		liblsb_found=no
	elif test x"${with_liblsb}" != x; then
        CPPFLAGS="$CPPFLAGS -I${with_liblsb}/include"
        AC_CHECK_HEADER(liblsb.h, [clampi_liblsb_path=${with_liblsb}; liblsb_found=yes], [AC_MSG_ERROR([Can't find the libLSB header files in ${with_liblsb}])])
    fi

    if test x"${liblsb_found}" == xyes; then
        AC_DEFINE(HAVE_LIBLSB, 1, enables libLSB)
        AC_MSG_NOTICE([libLSB support enabled])
        if test x${clampi_liblsb_path} != x; then
            CXXFLAGS="${CXXFLAGS} -I${with_liblsb}/"
            LDFLAGS="${LDFLAGS} -L${clampi_liblsb_path}/lib"
        fi
    fi
    ]
)


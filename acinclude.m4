dnl Checks lib $1 for function $2, setting $3 to 1 or 0 accordingly. If it
dnl doesn't find $2 it performs the same for function $4 and variable $5.
AC_DEFUN([TH_CHECK_ALTERNATIVE_FUNCS],
[
    $3=0
    $5=0
    AC_CHECK_LIB($1, $2,
    [
	$3=1
    ],
    [
	AC_CHECK_LIB($1, $4,
	[
	    $5=1
	],
	[
	    AC_MSG_ERROR([Unable to find functions $2 or $4 in lib $1])
	])
    ])
    AC_DEFINE_UNQUOTED([$3], [$$3], [Alternative function in lib $1])
    AC_DEFINE_UNQUOTED([$5], [$$5], [Alternative function in lib $1])
])

# Recursively evaluates a variable until
# it contains no references to other variables
AC_DEFUN([TH_EXPAND_VARS],
[
	while echo $$1 | grep -q '\$'
	do
		$1="`eval echo $$1`"
	done
])


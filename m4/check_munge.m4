# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Commercial License Information:
#
# PBS Pro is commercially licensed software that shares a common core with
# the OpenPBS software.  For a copy of the commercial license terms and
# conditions, go to: (http://www.pbspro.com/agreement.html) or contact the
# Altair Legal Department.
#
# Altair's dual-license business model allows companies, individuals, and
# organizations to create proprietary derivative works of OpenPBS and
# distribute them - whether embedded or bundled with other software -
# under a commercial license agreement.
#
# Use of Altair's trademarks, including but not limited to "PBS™",
# "OpenPBS®", "PBS Professional®", and "PBS Pro™" and Altair's logos is
# subject to Altair's trademark licensing policies.

AC_DEFUN([PBS_AC_CHECK_MUNGE],
  [
    HAVE_MUNGE=1
    MUNGE_PBS_SOCKET_PATH=
    MUNGE_PBS_LIBAUTH_NAME=munge
    munge_augment_name=
    AC_CHECK_HEADER([munge.h], [], [HAVE_MUNGE=])
    pbs_ac_save_libs=$LIBS
    AC_SEARCH_LIBS([munge_ctx_create], [munge], [], [HAVE_MUNGE=])
    LIBS=$pbs_ac_save_libs
    AC_ARG_WITH([munge-pbs-socket],
      [
        AS_HELP_STRING([--with-munge-pbs-socket=[PATH|NAME]],
          [absolute path to or augmented name for the PBS MUNGE socket (default: pbs)])
      ],
      [
        AS_IF([test -z "$HAVE_MUNGE"],
          [AC_MSG_ERROR([--with-munge-pbs-socket specified, but MUNGE is not installed.])])
        AS_CASE([$withval],
          [/*], [MUNGE_PBS_SOCKET_PATH=$withval],
          [no], [MUNGE_PBS_SOCKET_PATH=],
          [yes], [munge_augment_name=pbs],
          [munge_augment_name=$withval])
      ])
    AC_ARG_WITH([munge-pbs-libauth-name],
      [
        AS_HELP_STRING([--with-munge-libauth-name=[NAME]],
          [string to include authentication library name for MUNGE (default: munge)])
      ],
      [
        AS_CASE([$withval],
          [/*], [MUNGE_PBS_LIBAUTH_NAME=$withval],
          [no|yes], [MUNGE_PBS_LIBAUTH_NAME=],
          [MUNGE_PBS_LIBAUTH_NAME=$withval])
      ])
    AS_IF([test -n "$munge_augment_name"],
      [
        AC_PATH_PROG([MUNGED], [munged], [],
          [$PATH$PATH_SEPARATOR/usr/sbin$PATH_SEPARATOR/sbin$PATH_SEPARATOR])
        AS_IF([test -z "$MUNGED"], [AC_MSG_ERROR([munged program not found])])
        dnl use quadrigraphs in regex to avoid autoconf quoting issues
        munge_augment_re='.*\@<:@\(.*\)\(/@<:@^.@:>@*\)\(\.@<:@^@:>@@:>@*\)\@:>@'
        MUNGE_PBS_SOCKET_PATH=`$MUNGED --help 2>&1 | \
          $GREP '@<:@-@:>@-socket=' | \
          $SED -e "s,$munge_augment_re,\1\2-$munge_augment_name\3,"`
      ])
    AS_IF([test -n "$HAVE_MUNGE"],
      [
        AC_DEFINE([HAVE_MUNGE], [1], [The MUNGE header and library are present])
        AC_MSG_NOTICE([MUNGE socket path used by PBS: ${MUNGE_PBS_SOCKET_PATH:-(system default)}])
        AC_SUBST([MUNGE_PBS_LIBAUTH_NAME], [$MUNGE_PBS_LIBAUTH_NAME])
        AC_MSG_NOTICE([MUNGE authentication library set to libauth_${MUNGE_PBS_LIBAUTH_NAME}])
      ])
    AS_IF([test -n "$MUNGE_PBS_SOCKET_PATH"],
      [
        AC_DEFINE_UNQUOTED([MUNGE_PBS_SOCKET_PATH], ["$MUNGE_PBS_SOCKET_PATH"],
          [MUNGE socket path to use for PBS user authentication])
      ])
    AM_CONDITIONAL([HAVE_MUNGE], [test -n "$HAVE_MUNGE"])
  ])

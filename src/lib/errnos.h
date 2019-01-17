#define mk(ERRNO) case ERRNO: label = #ERRNO; break;
mk(EPERM)
mk(ENOENT)
mk(ESRCH)
mk(EINTR)
mk(EIO)
mk(ENXIO)
mk(E2BIG)
mk(ENOEXEC)
mk(EBADF)
mk(ECHILD)
mk(EAGAIN)
mk(ENOMEM)
mk(EACCES)
mk(EFAULT)
mk(ENOTBLK)
mk(EBUSY)
mk(EEXIST)
mk(EXDEV)
mk(ENODEV)
mk(ENOTDIR)
mk(EISDIR)
mk(EINVAL)
mk(ENFILE)
mk(EMFILE)
mk(ENOTTY)
mk(ETXTBSY)
mk(EFBIG)
mk(ENOSPC)
mk(ESPIPE)
mk(EROFS)
mk(EMLINK)
mk(EPIPE)
mk(EDOM)
mk(ERANGE)
mk(EDEADLK)
mk(ENAMETOOLONG)
mk(ENOLCK)
mk(ENOSYS)
mk(ENOTEMPTY)
mk(ELOOP)
//mk(EWOULDBLOCK)
mk(ENOMSG)
mk(EIDRM)

// also defined in MidWay.h if OS don't define these
mk(EBADRQC)


mk(ENOSTR)
mk(ENODATA)
mk(ETIME)
mk(ENOSR)
mk(ENONET)
mk(EREMOTE)
mk(ENOLINK)
mk(EBADMSG)
mk(EOVERFLOW)

// linux only?
/* mk(ECHRNG) */
/* mk(EL2NSYNC) */
/* mk(EL3HLT) */
/* mk(EL3RST) */
/* mk(ELNRNG) */
/* mk(EUNATCH) */
/* mk(ENOCSI) */
/* mk(EL2HLT) */
/* mk(EBADE) */
/* mk(EBADR) */
/* mk(EXFULL) */
/* mk(ENOANO) */
/* mk(EBADSLT) */
/* mk(EDEADLOCK) */
/* mk(EBFONT) */
/* mk(ENOPKG) */
/* mk(EADV) */
/* mk(ESRMNT) */
/* mk(ECOMM) */

 
mk(EPROTO)
mk(EMULTIHOP)

//mk(EDOTDOT)
//mk(ENOTUNIQ)
//mk(EBADFD)
//mk(EREMCHG)
mk(ELIBACC)
//mk(ELIBBAD)
//mk(ELIBSCN)
//mk(ELIBMAX)
//mk(ELIBEXEC)
mk(EILSEQ)
//mk(ERESTART)
//mk(ESTRPIPE)
mk(EUSERS)
mk(ENOTSOCK)
mk(EDESTADDRREQ)
mk(EMSGSIZE)
mk(EPROTOTYPE)
mk(ENOPROTOOPT)
mk(EPROTONOSUPPORT)
mk(ESOCKTNOSUPPORT)
mk(EOPNOTSUPP)
mk(EPFNOSUPPORT)
mk(EAFNOSUPPORT)
mk(EADDRINUSE)
mk(EADDRNOTAVAIL)
mk(ENETDOWN)
mk(ENETUNREACH)
mk(ENETRESET)
mk(ECONNABORTED)
mk(ECONNRESET)
mk(ENOBUFS)
mk(EISCONN)
mk(ENOTCONN)
mk(ESHUTDOWN)
mk(ETOOMANYREFS)
mk(ETIMEDOUT)
mk(ECONNREFUSED)
mk(EHOSTDOWN)
mk(EHOSTUNREACH)
mk(EALREADY)
mk(EINPROGRESS)
mk(ESTALE)
mk(EUCLEAN)
//mk(ENOTNAM)
mk(ENAVAIL)
//mk(EISNAM)
//mk(EREMOTEIO)
mk(EDQUOT)
//mk(ENOMEDIUM)
//mk(EMEDIUMTYPE)
mk(ECANCELED)
//mk(ENOKEY)
///mk(EKEYEXPIRED)
//mk(EKEYREVOKED)
//mk(EKEYREJECTED)
//mk(EOWNERDEAD)
mk(ENOTRECOVERABLE)
//mk(ERFKILL)
//mk(EHWPOISON)
mk(ENOTSUP)
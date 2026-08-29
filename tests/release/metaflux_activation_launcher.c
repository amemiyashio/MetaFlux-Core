#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

static int fail(const char* operation) {
  const int error = errno;
  (void)fprintf(stderr, "metaflux-activation-launcher: %s: %s\n", operation, strerror(error));
  return 1;
}

int main(int argc, char** argv) {
  struct sockaddr_un address = {0};
  struct passwd* account = NULL;
  struct group* group = NULL;
  char pid_text[32] = {0};
  int listener = -1;
  int descriptor_flags = 0;

  if (argc != 3) {
    (void)fprintf(stderr, "usage: metaflux-activation-launcher SOCKET_PATH DAEMON\n");
    return 64;
  }
  if (geteuid() != 0) {
    errno = EPERM;
    return fail("launcher must start as root");
  }
  if (argv[1][0] != '/' || strlen(argv[1]) >= sizeof(address.sun_path)) {
    errno = ENAMETOOLONG;
    return fail("invalid socket path");
  }

  errno = 0;
  account = getpwnam("metaflux");
  if (account == NULL) {
    if (errno == 0) {
      errno = ENOENT;
    }
    return fail("resolve metaflux account");
  }
  errno = 0;
  group = getgrnam("metaflux");
  if (group == NULL) {
    if (errno == 0) {
      errno = ENOENT;
    }
    return fail("resolve metaflux group");
  }

  listener = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (listener < 0) {
    return fail("create SOCK_SEQPACKET listener");
  }
  address.sun_family = AF_UNIX;
  (void)memcpy(address.sun_path, argv[1], strlen(argv[1]) + 1U);
  if (bind(listener, (const struct sockaddr*)&address, sizeof(address)) != 0) {
    (void)close(listener);
    return fail("bind activation socket");
  }
  if (chown(argv[1], account->pw_uid, group->gr_gid) != 0 || chmod(argv[1], 0660) != 0 ||
      listen(listener, 128) != 0) {
    const int error = errno;
    (void)close(listener);
    (void)unlink(argv[1]);
    errno = error;
    return fail("configure activation socket");
  }

  if (listener != 3) {
    if (dup2(listener, 3) < 0) {
      const int error = errno;
      (void)close(listener);
      (void)unlink(argv[1]);
      errno = error;
      return fail("move listener to fd 3");
    }
    (void)close(listener);
    listener = 3;
  }
  descriptor_flags = fcntl(listener, F_GETFD);
  if (descriptor_flags < 0 || fcntl(listener, F_SETFD, descriptor_flags & ~FD_CLOEXEC) != 0) {
    const int error = errno;
    (void)close(listener);
    (void)unlink(argv[1]);
    errno = error;
    return fail("clear close-on-exec on fd 3");
  }

  if (snprintf(pid_text, sizeof(pid_text), "%ld", (long)getpid()) <= 0 ||
      setenv("LISTEN_PID", pid_text, 1) != 0 || setenv("LISTEN_FDS", "1", 1) != 0 ||
      setenv("LISTEN_FDNAMES", "metafluxd", 1) != 0 || setenv("METAFLUX_SOCKET", argv[1], 1) != 0) {
    const int error = errno;
    (void)close(listener);
    (void)unlink(argv[1]);
    errno = error;
    return fail("publish activation environment");
  }

  if (initgroups(account->pw_name, group->gr_gid) != 0 || setgid(group->gr_gid) != 0 ||
      setuid(account->pw_uid) != 0) {
    const int error = errno;
    (void)close(listener);
    (void)unlink(argv[1]);
    errno = error;
    return fail("drop to metaflux account");
  }

  {
    char* const daemon_arguments[] = {argv[2], NULL};
    execv(argv[2], daemon_arguments);
  }

  {
    const int error = errno;
    (void)close(listener);
    (void)unlink(argv[1]);
    errno = error;
  }
  return fail("exec packaged metafluxd");
}

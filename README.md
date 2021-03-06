Flytrap
=======

Flytrap is a simple network scan detection and mitigation tool
developed at the University of Oslo as a replacement for LaBrea.

Flytrap listens on a network interface for unanswered ARP requests and
assumes the identities of the requested hosts.  It then logs all
traffic to that address, and optionally responds to TCP connection
attempts in order to slow down the scanner.

The latest version of the source code is available from Github:

  https://github.com/unioslo/flytrap

Bugs can be reported as issues in Github:

  https://github.com/unioslo/flytrap/issues

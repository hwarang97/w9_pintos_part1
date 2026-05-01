#ifndef __LIB_SYSCALL_NR_H
#define __LIB_SYSCALL_NR_H

/* 시스템 콜 번호. */
enum {
	/* 프로젝트 2 이후. */
	SYS_HALT,                   /* 운영체제를 정지한다. */
	SYS_EXIT,                   /* 현재 프로세스를 종료한다. */
	SYS_FORK,                   /* 현재 프로세스를 복제한다. */
	SYS_EXEC,                   /* 현재 프로세스를 다른 실행 파일로 전환한다. */
	SYS_WAIT,                   /* 자식 프로세스가 종료될 때까지 기다린다. */
	SYS_CREATE,                 /* 파일을 만든다. */
	SYS_REMOVE,                 /* 파일을 삭제한다. */
	SYS_OPEN,                   /* 파일을 연다. */
	SYS_FILESIZE,               /* 파일 크기를 얻는다. */
	SYS_READ,                   /* 파일에서 읽는다. */
	SYS_WRITE,                  /* 파일에 쓴다. */
	SYS_SEEK,                   /* 파일 내 현재 위치를 변경한다. */
	SYS_TELL,                   /* 파일 내 현재 위치를 보고한다. */
	SYS_CLOSE,                  /* 파일을 닫는다. */

	/* 프로젝트 3 및 선택적으로 프로젝트 4. */
	SYS_MMAP,                   /* 파일을 메모리에 매핑한다. */
	SYS_MUNMAP,                 /* 메모리 매핑을 제거한다. */

	/* 프로젝트 4 전용. */
	SYS_CHDIR,                  /* 현재 디렉터리를 변경한다. */
	SYS_MKDIR,                  /* 디렉터리를 만든다. */
	SYS_READDIR,                /* 디렉터리 엔트리를 읽는다. */
	SYS_ISDIR,                  /* fd가 디렉터리를 나타내는지 검사한다. */
	SYS_INUMBER,                /* fd의 inode 번호를 반환한다. */
	SYS_SYMLINK,                /* symbolic link를 만든다. */

	/* 프로젝트 2 추가 과제. */
	SYS_DUP2,                   /* 파일 디스크립터를 복제한다. */

	SYS_MOUNT,
	SYS_UMOUNT,
};

#endif /* lib/syscall-nr.h */

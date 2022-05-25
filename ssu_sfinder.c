#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

// 명령어 함수만 제외하고 나머지 기능들은 헤더파일로..
#include "ssu_sfinder.h"

int tokenize(char *input, char *argv[]);
void fmd5(int argc, char *argv[]);
void help(void);

int main(void)
{
	while (1) {
		char input[STRMAX];
		char *argv[ARGMAX];

		int argc = 0;
		
		printf("20192209> ");
		fgets(input, sizeof(input), stdin);
		input[strlen(input) - 1] = '\0';

		argc = tokenize(input, argv);

		if (argc == 0)
			continue;

		else if (!strcmp(argv[0], "exit"))
			break;

		else if (!strcmp(argv[0], "fmd5")) 
			fmd5(argc, argv);

		else
			help();

	}

	printf("Prompt End\n");

	exit(0);
}

int tokenize(char *input, char *argv[])
{
	char *ptr = NULL;
	int argc = 0;
	ptr = strtok(input, " ");

	while (ptr != NULL) {
		argv[argc++] = ptr;
		ptr = strtok(NULL, " ");
	}

	return argc;
}

void fmd5(int argc, char *argv[])
{
//	int threadNum;
	int opt = 0;
	extern char *optarg;

	char target_dir[PATHMAX];
	dirList *dirlist = (dirList *)malloc(sizeof(dirList)); // 지역변수로..
	dups_list_h = (fileList *)malloc(sizeof(fileList)); // 전역변수 사용

	memset(dirlist, 0, sizeof(dirlist));
	dirlist->next = NULL;


	optind = 1;

	while ((opt = getopt(argc, argv, "e:l:h:d:t:")) != -1) // 모든 옵션에 인자입력요구
	{
		switch(opt) 
		{
			case 'e': // 파일 확장자 옵션
				printf("e: extension %s\n", optarg);
				break;
			case 'l': // 파일 최소 사이즈
				printf("l: minsize %s\n", optarg);
				break;
			case 'h':
				printf("h: maxsize %s\n", optarg);
				break;
			case 'd':
				printf("d: target_directory %s\n", optarg);
				break;
			case 't':
				printf("쓰레드 구현해야함\n");
				break;
			case '?': // 다른 옵션 들어온 경우
				printf("invalid option!\n");
				return;
		}
	}

	if (check_args(argc, argv)) // 옵션 에러 처리
		return;

	if (strchr(argv[8], '~') != NULL) // home을 포함한 상대경로
		get_path_from_home(argv[8], target_dir);
	else // 절대경로
		realpath(argv[8], target_dir);

//	if (argc == 11) // 쓰레드의 개수를 인자로 받은 경우
//		threadNum = atoi(argv[10]);
//	else // 메인 쓰레드 하나 존재
//		threadNum = 1;

//	pthread_t tid[threadNum];

	get_same_size_files_dir();

	struct timeval begin_t, end_t;

	gettimeofday(&begin_t, NULL);

	dirlist_append(dirlist, target_dir);


	multiArg *arg = (multiArg *)malloc(sizeof(multiArg));
//	arg->dirlist = dirlist;
//	arg->threadNum = threadNum;
	dir_traverse(dirlist);

	find_duplicates();
//	remove_no_duplicates();

	gettimeofday(&end_t, NULL);

	end_t.tv_sec -= begin_t.tv_sec;

	if (end_t.tv_usec < begin_t.tv_usec) {
		end_t.tv_sec--;
		end_t.tv_usec += 1000000;
	}

	end_t.tv_usec -= begin_t.tv_usec;

	if (dups_list_h->next == NULL)
		printf("No duplicates in %s\n", target_dir);
	else
		filelist_print_format(dups_list_h);

	printf("Searching time %ld:%06ld(sec:usec)\n\n", end_t.tv_sec, end_t.tv_usec);

	get_trash_dir();
	printf("trash : %s\n", trash_path);

	delete_prompt();

	return;
}

void help(void)
{
	printf("Usage:\n");
	printf("  > fmd5 -e [FILE_EXTENSION] -l [MINSIZE] -h [MAXSIZE] -d [TARGET_DIRECTORY] -t [THREAD_NUM]\n");
	printf("     >> delete -l [SET_INDEX] -d [OPTARG] -i -f -t\n");
	printf("  > trash -c [CATEGORY] -o [ORDER]\n");
	printf("  > restore [RESTORE_INDEX]\n");
	printf("  > help\n");
	printf("  > exit\n\n");
}

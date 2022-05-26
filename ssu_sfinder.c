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
void list(int argc, char *argv[]);
void help(void);

int main(void)
{
	dups_list_h = (fileList *)malloc(sizeof(fileList));
	
	memset(dups_list_h, 0, sizeof(dups_list_h));
	dups_list_h->next = NULL;

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

		else if (!strcmp(argv[0], "list"))
			list(argc, argv);

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
	int err = 0;
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
				break;
			case 'l': // 파일 최소 사이즈
				break;
			case 'h':
				break;
			case 'd':
				break;
			case 't':
				break;
			case '?': // 다른 옵션 들어온 경우
				err = -1;
				break;
		}
	}

	if (err == -1) // 잘못된 옵션 에러 처리
		return;

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


//	multiArg *arg = (multiArg *)malloc(sizeof(multiArg));
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

	delete_prompt();

	return;
}

void list(int argc, char *argv[])
{
	if (dups_list_h->next == NULL) {
		printf("ERROR: [fmd5] command must be executed before [list]\n");
		return;
	}

	int opt = 0;
	int flag_l = 0; // LIST_TYPE 플래그, 디폴트 "fileset"
	int flag_c = 0; // CATEGORY 플래그, 디폴트 "size"
	int flag_o = 1; // ORDER 플래그, 디폴트 1(오름차순)
	int err = 0;
	char category[NAMEMAX];

	optind = 1;

	while ((opt = getopt(argc, argv, "l:c:o:")) != -1)
	{
		switch(opt)
		{
			case 'l': // LIST_TYPE
				if (!strcmp(optarg, "filelist")) // 옵션이 파일리스트
					flag_l = 1;
				else // fileset 또는 다른 값을 입력받는 경우(디폴트 처리)
					flag_l = 0;
				break;
			case 'c': // CATEGORY
				if (!strcmp(optarg, "filename"))
					flag_c = 1;
				else // size 입력 또는 그 외의 값이 입력받은 경우
					strcpy(category, optarg);
				break;
			case 'o':
				if (!strcmp(optarg, "-1")) // 내림차순
					flag_o = -1;
				else // 오름차순
					flag_o = 1;
				break;
			case '?':
				err = -1;
				break;
		}
	}

	if (err == -1)
		return;

	/* 인자 입력개수 처리 해야함 */
	
	// filelist 정렬하는 경우 (l옵션 입력값 filelist)
	if (flag_l == 1) {
		if (flag_c == 1) { // filename으로 정렬
			if (flag_o == -1) // 내림차순 정렬
				name_down_sort(dups_list_h);
			else
				name_up_sort(dups_list_h);
		}
		else if (flag_c == 0) { // stat buf이용하여 정렬하는 경우
			if (!strcmp(category, "size") || !strcmp(category, "uid") || !strcmp(category, "gid") || !strcmp(category, "mode") || !strcmp(category, "")) {
				if (flag_o == -1)  // 내림차순 정렬
					category_down_sort(dups_list_h, category);
				else               // 오름차순 정렬
					category_up_sort(dups_list_h, category);
			}
			else {
				printf("ERROR: -c option must be [filename] or [size] or [uid] or [gid] or [mode]\n");
				return;
			}
		}
	}
	else {// fileset 정렬
		if (flag_c != 0) {// size외의 다른 옵션을 입력받은 경우
			printf("fileset sort: -c [option] only available [size]\n");
			return;
		}

		if (flag_o == -1) {// 만약 내림차순 정렬 입력받은 경우
			fileset_down_sort(dups_list_h);
		}
		else
			fileset_up_sort(dups_list_h);
	}

	filelist_print_format(dups_list_h);

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

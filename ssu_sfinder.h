#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#include <openssl/md5.h>

#define NAMEMAX 255
#define PATHMAX 4096
#define HASHMAX 33

#define STRMAX 10000
#define ARGMAX 11

typedef struct fileInfo {
	char path[PATHMAX];
	struct stat statbuf;
	struct fileInfo *next;
} fileInfo;

typedef struct fileList {
	long long filesize;
	char hash[HASHMAX];
	fileInfo *fileInfoList;
	struct fileList *next;
} fileList;

typedef struct dirList {
	char dirpath[PATHMAX];
	struct dirList *next;
} dirList;

#define DIRECTORY 1
#define REGFILE 2

#define KB 1024
#define MB 1024*1024
#define GB 1024*1024*1024
#define SIZE_ERROR -2

char extension[10]; // 확장자
char same_size_files_dir[PATHMAX];
// char trash_path[PATHMAX];
long long minbsize;
long long maxbsize;
fileList *dups_list_h;


long long get_size(char *filesize);
// "*.(확장자)"를 입력받은 경우 확장자 부분만 가져오는 함수
char *get_extension(char *filename)
{
     char *tmp_ext;
 
     if ((tmp_ext = strstr(filename, ".tar")) != NULL || (tmp_ext = strrchr(filename, '.')    ) != NULL)
         return tmp_ext + 1;
     else
         return NULL;
}


void get_path_from_home(char *path, char *path_from_home)
{
	char path_without_home[PATHMAX] = {0, };
	char *home_path;

	home_path = getenv("HOME");

	if (strlen(path) == 1) // "~"만 입력받은 경우
		strncpy(path_from_home, home_path, strlen(home_path));
	else { // "~/path로 입력받은 경우"
		strncpy(path_without_home, path + 1, strlen(path) - 1);
		sprintf(path_from_home, "%s%s", home_path, path_without_home);
	}
}

int is_dir(char *target_dir)
{
	struct stat statbuf;

	if (lstat(target_dir, &statbuf) < 0) {
		printf("ERROR: lstat error for %s\n", target_dir);
		return 1;
	}

	return S_ISDIR(statbuf.st_mode) ? DIRECTORY : 0;
}

// argument 에러처리
int check_args(int argc, char *argv[])
{
	char target_dir[PATHMAX];
	
	if (argc != 11) { // 옵션 값이 부족한 경우
		printf("Usage: fmd5 -e [FILE_EXTENSION] -l [MINSIZE] -h [MAXSIZE] -d [TARGET_DIRECTORY] -t [THREAD_NUM]\n");
		return 1;
	}

	/* 파일 확장자에 "*", "*.c" 만 입력받기 */
	if (strchr(argv[2], '*') == NULL) {
		printf("ERROR: [FILE_EXTENSION] should be '*' or '*.extension'\n");
		return 1;
	}

	if (strchr(argv[2], '.') != NULL) { // "*.(확장자)" 로 입력받은 경우
		strcpy(extension, get_extension(argv[2])); // 확장자부분만 가져옴

		if (strlen(extension) == 0) { // 입력받은 확장자가 없는 경우
			printf("ERROR: There should be extension\n");
			return 1;
		}
	}

	/* 파일 크기 인자 처리 */
	minbsize = get_size(argv[4]);

	if (minbsize == -1)
		minbsize = 0;

	maxbsize = get_size(argv[6]);

	if (minbsize == SIZE_ERROR || maxbsize == SIZE_ERROR) {
		printf("ERROR: Size wrong -min size : %s -max size : %s\n", argv[4], argv[6]);
		return 1;
	}

	if (maxbsize != -1 && minbsize > maxbsize) {
		printf("ERROR: [MAXSIZE] should be bigger than [MINSIZE]\n");
		return 1;
	}

	/* 탐색할 디렉토리 경로 인자 처리 */
	if (strchr(argv[8], '~') != NULL) // "~(홈  디렉토리)"가 포함된 경우
		get_path_from_home(argv[8], target_dir);
	else { // target_dir의 절대경로
		if (realpath(argv[8], target_dir) == NULL) {
			printf("ERROR: [TARGET_DIRECTORY] should exist\n");
			return 1;
		}
	}

	if (access(target_dir, F_OK) == -1) { // 존재하지 않는 디렉토리인 경우
		printf("ERROR: %s directory doesn't exist\n", target_dir);
		return 1;
	}

	if (!is_dir(target_dir)) { // 디렉토리가 아닌 경우
		printf("ERROR: [TARGET_DIRECTORY] should be a directory\n");
		return 1;
	}

	return 0;
}

// 파일 사이즈 가져오는 함수
long long get_size(char *filesize)
{
	double size_bytes = 0;
	char size[STRMAX] = {0, };
	char size_unit[4] = {0, };
	int size_idx = 0;

	if (!strcmp(filesize, "~")) // 모든 파일사이즈인 경우("~")
		size_bytes = -1;
	else {
		for (int i = 0; i < strlen(filesize); i++) {
			if (isdigit(filesize[i]) || filesize[i] == '.') { // 숫자거나 . 인 경우
				size[size_idx++] = filesize[i];
				if (filesize[i] == '.' && !isdigit(filesize[i + 1]))
					return SIZE_ERROR;
			}
			else { // 파일 단위인 경우
				strcpy(size_unit, filesize + i);
				break;
			}
		}

		size_bytes = atof(size);

		if (strlen(size_unit) != 0) {
			if (!strcmp(size_unit, "kb") || !strcmp(size_unit, "KB"))
				size_bytes *= KB;
			else if (!strcmp(size_unit, "mb") || !strcmp(size_unit, "MB"))
				size_bytes *= MB;
			else if (!strcmp(size_unit, "gb") || !strcmp(size_unit, "GB"))
				size_bytes *= GB;
			else
				return SIZE_ERROR;
		}
	}

	return (long long)size_bytes;
}

void dirlist_append(dirList *head, char *path)
{
	dirList *newFile = (dirList *)malloc(sizeof(dirList));

	strcpy(newFile->dirpath, path);
	newFile->next = NULL;

	if (head->next == NULL) // dirlist에 처음 append 되는 경우
		head->next = newFile;
	else{ // dirlist가 기존에 존재하는 경우
		dirList *cur = head->next;

		while(cur->next != NULL) // dirlist의 마지막 노드까지
			cur = cur->next;

		cur->next = newFile; // 마지막 노드의 next 포인터를 newFile로..
	}
}

/* target_dir에 있는 파일 목록을 뽑아 dirent구조체의 namelist에 저장 */
int get_dirlist(char *target_dir, struct dirent ***namelist)
{
	int cnt = 0;

	// 파일목록을 아스키코드 순으로 namelist에 저장
	if ((cnt = scandir(target_dir, namelist, NULL, alphasort)) == -1) {
		printf("ERROR: scandir error for %s\n", target_dir);
		return -1;
	}

	return cnt;
}

/* 파일명을 받아 파일모드를 리턴하는 함수 */
int get_file_mode(char *target_file, struct stat *statbuf)
{
	if (lstat(target_file, statbuf) < 0) {
		printf("ERROR: lstat error for %s\n", target_file);
		return 0;
	}

	if (S_ISREG(statbuf->st_mode)) // 정규파일인 경우
		return REGFILE;
	else if (S_ISDIR(statbuf->st_mode)) // 디렉토리인 경우
		return DIRECTORY;
	else
		return 0;
}

/* 디렉토리와 파일명을 받아 전체경로를 알아내는 함수 */
void get_fullpath(char *target_dir, char *target_file, char *fullpath)
{
	strcat(fullpath, target_dir);

	if (fullpath[strlen(target_dir) - 1] != '/') // 경로의 마지막 문자열이 '/'이 아닌 경우
		strcat(fullpath, "/");

	strcat(fullpath, target_file); // 폴더 경로의 끝에 파일명 이어붙임
	fullpath[strlen(fullpath)] = '\0';
}

/* 폴더 내 파일 제거 */
void remove_files(char *dir)
{
    struct dirent **namelist;
    int listcnt = get_dirlist(dir, &namelist);

    for (int i = 0; i < listcnt; i++) {
        char fullpath[PATHMAX] = {0, };

        if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
            continue;

        get_fullpath(dir, namelist[i]->d_name, fullpath);

        remove(fullpath);
    }
}

/* 같은 사이즈 파일 보관하는 폴더 생성 */
void get_same_size_files_dir(void)
{
    get_path_from_home("~/20192209", same_size_files_dir);
  
    // 폴더 초기화
    if (access(same_size_files_dir, F_OK) == 0)
        remove_files(same_size_files_dir);
    else
        mkdir(same_size_files_dir, 0755);
} 

// md5 해쉬값 구하는 함수
int md5_hash(char *target_path, char *hash_result)
{
	FILE *fp;
	unsigned char hash[MD5_DIGEST_LENGTH];
	unsigned char buffer[SHRT_MAX];
	int bytes = 0;
	MD5_CTX md5;

	if ((fp = fopen(target_path, "rb")) == NULL){
		printf("ERROR: fopen error for %s\n", target_path);
		return 1;
	}

	MD5_Init(&md5);

	while ((bytes = fread(buffer, 1, SHRT_MAX, fp)) != 0)
		MD5_Update(&md5, buffer, bytes);
					
	MD5_Final(hash, &md5);

	for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
		sprintf(hash_result + (i * 2), "%02x", hash[i]);
	hash_result[HASHMAX-1] = 0;

	fclose(fp);

	return 0;
}

void dirlist_delete_all(dirList *head)
{
	dirList *dirlist_cur = head->next;
	dirList *tmp;

	while (dirlist_cur != NULL) {
		tmp = dirlist_cur->next;
		free(dirlist_cur);
		dirlist_cur = tmp;
	}

	head->next = NULL;
}

void dir_traverse(dirList *dirlist)
{
	dirList *cur = dirlist->next;
	dirList *subdirs = (dirList *)malloc(sizeof(dirList));

	memset(subdirs, 0, sizeof(dirList));

	while (cur != NULL) { // dirlist의 모든 노드 순회
		struct dirent **namelist;
		int listcnt;

		listcnt = get_dirlist(cur->dirpath, &namelist);

		for (int i = 0; i < listcnt; i++) { // 폴더 내의 각 파일마다 반복
			char fullpath[PATHMAX] = {0, };
			struct stat statbuf;
			int file_mode;
			long long filesize;

			if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
				continue;

			get_fullpath(cur->dirpath, namelist[i]->d_name, fullpath);

			// "proc", "run", "sys" 디렉토리 제외
			if (!strcmp(fullpath, "/proc") || !strcmp(fullpath, "/run") || !strcmp(fullpath, "/sys"))
				continue;

			file_mode = get_file_mode(fullpath, &statbuf);

			// 파일사이즈 0인 파일 제외
			if ((filesize = (long long)statbuf.st_size) == 0)
				continue;

			if (filesize < minbsize)
				continue;

			if (maxbsize != -1 && filesize > maxbsize)
				continue;

			if (file_mode == DIRECTORY) // target 폴더에 하위폴더들이 있는 경우 subdirs 리스트로 관리하여 뒤쪽 노드에 계속 추가
				dirlist_append(subdirs, fullpath);
			else if (file_mode == REGFILE) { // 정규파일의 경우
				FILE *fp;
				char filename[PATHMAX*2];
				char *path_extension;
				char hash[HASHMAX];

				sprintf(filename, "%s/%lld", same_size_files_dir, filesize);

				memset(hash, 0, HASHMAX);
				md5_hash(fullpath, hash);

				path_extension = get_extension(fullpath);

				if (strlen(extension) != 0) {
					if (path_extension == NULL || strcmp(extension, path_extension))
						continue;
				}

				if ((fp = fopen(filename, "a")) == NULL) {
					printf("ERROR: fopen error for %s\n", filename);
					return;
				}

				fprintf(fp, "%s %s\n", hash, fullpath);

				fclose(fp);
			}
		}

		cur = cur->next;
	}

	dirlist_delete_all(dirlist);

	if (subdirs->next != NULL) // dirlist의 하위 디렉토리가 있다면 재귀방식으로 순회
		dir_traverse(subdirs);
}

/* fileInfo 리스트에 fileinfo 구조체 추가하는 함수 */
void fileinfo_append(fileInfo *head, char *path)
{
	fileInfo *fileinfo_cur;

	fileInfo *newinfo = (fileInfo *)malloc(sizeof(fileInfo));
	memset(newinfo, 0, sizeof(fileInfo));

	// newinfo 구조체에 대한 초기값 설정
	strcpy(newinfo->path, path);
	lstat(newinfo->path, &newinfo->statbuf);
	newinfo->next = NULL;

	if (head->next == NULL) // fileinfo 리스트의 맨 처음 append 하는 경우
		head->next = newinfo;
	else { // fileinfo 리스트에 기존 데이터가 있는 경우
		fileinfo_cur = head->next;
		while (fileinfo_cur->next != NULL) // 맨 뒤로 이동하는 반복문
			fileinfo_cur = fileinfo_cur->next;

		fileinfo_cur->next = newinfo; // 마지막 노드의 next에 추가(마지막에 추가)
	}
}

/* 파일리스트에 새로운 노드(파일) 추가하는 함수 */
void filelist_append(fileList *head, long long filesize, char *path, char *hash)
{
	fileList *newfile = (fileList *)malloc(sizeof(fileList));
	memset(newfile, 0, sizeof(fileList));

	newfile->filesize = filesize;
	strcpy(newfile->hash, hash);
	
	newfile->fileInfoList = (fileInfo *)malloc(sizeof(fileInfo)); // fileInfo 정보를 위한 구조체
	memset(newfile->fileInfoList, 0, sizeof(fileInfo)); // fileInfo 초기화

	fileinfo_append(newfile->fileInfoList, path); // path정보를 받아 fileInfoList에 추가
	newfile->next = NULL;

	if (head->next == NULL) { // 맨 처음 리스트에 fileList의 노드 추가
		head->next = newfile;
	}
	else {
		fileList *cur_node = head->next, *prev_node = head, *next_node;

		// 노드의 마지막까지거나, 새로운 파일의 사이즈가 현재 노드보다 작을 때까지 이동
		while (cur_node != NULL && cur_node->filesize < newfile->filesize) {
			prev_node = cur_node;
			cur_node = cur_node->next;
		}

		// 현재 노드가 마지막이거나, newfile의 크기가 현재 노드보다 작은 경우 앞에 추가 
		newfile->next = cur_node;
		prev_node->next = newfile;
	}
}

/* 파일리스트에서 hash값이 동일한 파일의 인덱스 리턴하는 함수 */
int filelist_search(fileList *head, char *hash)
{
	fileList *cur = head;
	int idx = 0;

	while (cur != NULL) { // 반복문 돌면서 hash값이 동일한 인덱스 리턴
		if (!strcmp(cur->hash, hash))
			return idx;
		cur = cur->next;
		idx++;
	}

	return 0;
}

/* 중복파일 찾는 함수 */
void find_duplicates(void)
{
	struct dirent **namelist;
	int listcnt;
	char hash[HASHMAX];
	FILE *fp;

	listcnt = get_dirlist(same_size_files_dir, &namelist); // 중복파일목록의 폴더에서..

	for (int i = 0; i < listcnt; i++) { // 똑같은 크기의 파일세트마다..
		char filename[PATHMAX*2];
		long long filesize;
		char filepath[PATHMAX];
		char hash[HASHMAX];
		char line[STRMAX];

		if (!strcmp(namelist[i]->d_name, ".") || !strcmp(namelist[i]->d_name, ".."))
			continue;

		filesize = atoll(namelist[i]->d_name); // 파일이름이 파일 사이즈임
		sprintf(filename, "%s/%s", same_size_files_dir, namelist[i]->d_name);

		if ((fp = fopen(filename, "r")) == NULL) { // 파일 세트 목록들어있는 파일 open
			printf("ERROR: fopen error for %s\n", filename);
			continue;
		}

		while (fgets(line, sizeof(line), fp) != NULL) {
			int idx;

			strncpy(hash, line, HASHMAX);
			hash[HASHMAX - 1] = '\0';

			strcpy(filepath, line+HASHMAX);
			filepath[strlen(filepath) - 1] = '\0';

			if ((idx = filelist_search(dups_list_h, hash)) == 0) // 동일한 해시값이 없는 경우
				filelist_append(dups_list_h, filesize, filepath, hash);
			else { // 동일한 해시값이 이미 목록에 존재하는 경우(중복)
				fileList *filelist_cur = dups_list_h;

				// 해당 인덱스까지 이동 후, 해당 인덱스의 바로 뒤에 append
				while (idx--) {
					filelist_cur = filelist_cur->next;
				}
				fileinfo_append(filelist_cur->fileInfoList, filepath);
			}
		}

		fclose(fp);
	}
}

void sec_to_ymdt(struct tm *time, char *ymdt)
{
	sprintf(ymdt, "%04d-%02d-%02d %02d:%02d:%02d", time->tm_year + 1900, time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
}

void filesize_with_comma(long long filesize, char *filesize_w_comma)
{
	char filesize_wo_comma[STRMAX] = {0, };
	int comma;
	int idx = 0;

	sprintf(filesize_wo_comma, "%lld", filesize);
	comma = strlen(filesize_wo_comma)%3;

	for (int i = 0; i < strlen(filesize_wo_comma); i++) {
		if (i > 0 && (i%3) == comma)
			filesize_w_comma[idx++] = ',';

		filesize_w_comma[idx++] = filesize_wo_comma[i];
	}

	filesize_w_comma[idx] = '\0';
}

/* 파일 목록에서 정보 출력 */
void filelist_print_format(fileList *head)
{
	fileList *filelist_cur = head->next;
	int set_idx = 1;

	while (filelist_cur != NULL) { // 파일리스트의 모든 노드 순회
		fileInfo *fileinfolist_cur = filelist_cur->fileInfoList->next;
		char mtime[STRMAX];
		char atime[STRMAX];
		char filesize_w_comma[STRMAX] = {0, };
		int i = 1;

		filesize_with_comma(filelist_cur->filesize, filesize_w_comma);

		printf("---- Identical files #%d (%s bytes - %s) ----\n", set_idx++, filesize_w_comma, filelist_cur->hash);

		while (fileinfolist_cur != NULL) { // 중복파일에 대한 정보 fileinfo 리스트 순회
			sec_to_ymdt(localtime(&fileinfolist_cur->statbuf.st_mtime), mtime);
			sec_to_ymdt(localtime(&fileinfolist_cur->statbuf.st_atime), atime);
			printf("[%d] %s (mtime : %s) (atime : %s)\n", i++, fileinfolist_cur->path, mtime, atime);

			fileinfolist_cur = fileinfolist_cur->next;
		}
		printf("\n");

		filelist_cur = filelist_cur->next;
	}
}

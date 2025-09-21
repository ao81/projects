#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define MAX_LINE  260	// 1行当たりの最大長
#define MAX_PATHS 100	// 保存できる最大パス数
#define PATHSFILE "paths.txt"

// ターミナルをクリアする関数
void clear_screen() {
	system("cls");
}

// バッファをクリアする関数
void clear_input_buffer() {
	int ch;
	while ((ch = getchar()) != '\n' && ch != EOF);
}

// 入力待ち状態にする関数
void pause_continue(int type) {
	printf("\nEnterを押してメニューに戻ります。... ");
	if (type == 1) clear_input_buffer();
	getchar();
}

// \n を \0 に置き換える関数
void trim(char *str) {
	str[strcspn(str, "\n")] = '\0';
}

// ファイルから全てのパスを読み込む関数
int load_paths(char savedPaths[MAX_PATHS][MAX_LINE]) {
	FILE *fp = fopen(PATHSFILE, "r"); // ファイルを読み込みモードで開く
	if (!fp) return 0;

	int pathCount = 0;
	while (pathCount < MAX_PATHS && fgets(savedPaths[pathCount], MAX_LINE, fp)) {
		trim(savedPaths[pathCount]);
		if (strlen(savedPaths[pathCount]) > 0) pathCount++;
	}
	fclose(fp);
	return pathCount;
}

// パスをファイルに保存する関数
void save_paths(char savedPaths[MAX_PATHS][MAX_LINE], int pathCount) {
	FILE *fp = fopen(PATHSFILE, "w"); // ファイルを書き込みモードで開く
	if (!fp) {
		printf("ファイルの書き込みに失敗しました。\n");
		return;
	}

	for(int i = 0; i < pathCount; i++) {
		fprintf(fp, "%s\n", savedPaths[i]);
	}
	fclose(fp);

	// 隠しファイル属性を付与
	SetFileAttributes(PATHSFILE, FILE_ATTRIBUTE_HIDDEN);
}

// パスを追加する関数
void add_path() {
	clear_screen();
	clear_input_buffer();

	char newPath[MAX_LINE];
	printf("追加するパスを入力してください。(何も入力せずにEnterでキャンセル):\n> ");
	fgets(newPath, MAX_LINE, stdin);
	trim(newPath);

	if (strlen(newPath) == 0) {
		printf("\nキャンセルしました。\n");
		pause_continue(0);
		return;
	}

	FILE *fp = fopen(PATHSFILE, "a"); // ファイルを追記モードで開く
	if (!fp) {
		printf("ファイルを開けません。\n");
		pause_continue(0);
		return;
	}
	fprintf(fp, "%s\n", newPath);
	fclose(fp);

	// 隠しファイル属性を付与
	SetFileAttributes(PATHSFILE, FILE_ATTRIBUTE_HIDDEN);

	printf("\n[ %s ] を保存しました。\n", newPath);
	pause_continue(0);
}

// パスの一覧表示・削除を行う関数
void list_and_delete_paths() {
	clear_screen();
	clear_input_buffer();

	char savedPaths[MAX_PATHS][MAX_LINE];
	int pathCount = load_paths(savedPaths);
	if (pathCount == 0) {
		printf("まだパスは保存されていません。\n");
		pause_continue(0);
		return;
	}

	printf("--- 登録済みパス一覧 ---\n");
	for (int i = 0; i < pathCount; i++) {
		printf("%d: [ %s ]\n", i + 1, savedPaths[i]);
	}
	printf("------------------------\n");

	printf("\n削除したい番号を入力してください。(Enterでキャンセル): ");

	char deleteInput[16];
	if (!fgets(deleteInput, sizeof(deleteInput), stdin)) {
		printf("\n入力エラーです。\n");
		pause_continue(0);
		return;
	}

	trim(deleteInput);

	if (strlen(deleteInput) == 0) {
		printf("\nキャンセルしました。\n");
		pause_continue(0);
		return;
	}

	int deleteIndex = atoi(deleteInput);
	if (deleteIndex <= 0 || deleteIndex > pathCount) {
		printf("\n無効な値です。\n");
		pause_continue(0);
		return;
	}

	char deletedPath[MAX_LINE];
	strcpy(deletedPath, savedPaths[deleteIndex - 1]);

	for (int i = deleteIndex - 1; i < pathCount - 1; i++) {
		strcpy(savedPaths[i], savedPaths[i + 1]);
	}
	save_paths(savedPaths, pathCount - 1);

	printf("\n[ %s ] を削除しました。\n", deletedPath);
	pause_continue(0);
}

// 保存されているパスを元にユーザー環境変数Pathに追加/レジストリに反映する関数
void set_env() {
	clear_screen();
	clear_input_buffer();

	const char* envName = "Path";
	HKEY hKey; // レジストリキーを扱うハンドル用
	LONG result; // APIの結果確認用
	char currentPath[4096]; // 現在の Path を入れるバッファ
	DWORD pathSize = sizeof(currentPath); // バッファのサイズを Windows API に伝えるための変数

	// 現在の Path を取得
	result = RegOpenKeyEx(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
	if (result == ERROR_SUCCESS) {
		pathSize = sizeof(currentPath);
		result = RegQueryValueEx(hKey, envName, NULL, NULL, (LPBYTE)currentPath, &pathSize);
		if (result != ERROR_SUCCESS) currentPath[0] = '\0';
		RegCloseKey(hKey);
	}

	// 保存済みのパスを読み込み
	char savedPaths[MAX_PATHS][MAX_LINE];
	int pathCount = load_paths(savedPaths);
	if (pathCount == 0) {
		printf("パスが保存されていません。\n");
		pause_continue(0);
		return;
	}

	char pathsToAppend[4096] = "";
	char searchablePath[8192];
	sprintf(searchablePath, ";%s;", currentPath);

	// 重複の確認
	int addedCount = 0;
	for (int i = 0; i < pathCount; i++) {
		if (strlen(savedPaths[i]) == 0) continue;

		char searchKey[MAX_LINE + 2];
		sprintf(searchKey, ";%s;", savedPaths[i]);

		// searchablePath の中に searchKey が含まれているか
		if (!strstr(searchablePath, searchKey)) {
			if (addedCount > 0) strcat(pathsToAppend, ";");
			strcat(pathsToAppend, savedPaths[i]);
			addedCount++;
		}
	}

	if (addedCount == 0) {
		printf("追加する新しいパスはありませんでした。\n");
		pause_continue(0);
		return;
	}

	char updatePath[8192];
	if (strlen(currentPath) > 0) {
		sprintf(updatePath, "%s;%s", currentPath, pathsToAppend);
	} else {
		strcpy(updatePath, pathsToAppend);
	}

	// ユーザー環境変数レジストリキーを開き、書き込み可能にする
	result = RegOpenKeyEx(HKEY_CURRENT_USER, "Environment", 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey);

	if (result == ERROR_SUCCESS) {
		RegSetValueEx(hKey, envName, 0, REG_EXPAND_SZ, (const BYTE*)updatePath, (DWORD)(strlen(updatePath) + 1));

		// 環境設定が更新を即時反映するためにアプリに通知する
		SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment",SMTO_ABORTIFHUNG, 5000, NULL);
		printf("%d個の新しいパスを環境変数 '%s' に追加しました。\n", addedCount, envName);
	} else {
		printf("環境変数の設定に失敗しました。エラーコード: %ld\n", result);
	}
	RegCloseKey(hKey);

	pause_continue(0);
}

int main() {
	// コンソールの文字コードを UTF-8 にする
	SetConsoleOutputCP(65001);
	SetConsoleCP(65001);

	char menuChoice;

	while (1) {
		clear_screen();
		printf("====== 環境変数登録ツール ======\n");
		printf("--------------------------------\n");
		printf("1: パス保存\n");
		printf("2: パス一覧表示/削除\n");
		printf("3: 環境変数に登録\n\n");
		printf("q: 終了\n");
		printf("--------------------------------\n");
		printf("> ");

		if (scanf("%1c", &menuChoice) != 1) {
			clear_input_buffer();
			continue;
		}

		switch (menuChoice) {
			case '1': add_path(); break;
			case '2': list_and_delete_paths(); break;
			case '3': set_env(); break;
			case '\n': break;
			case 'q': return 0;
			default:
				printf("\n無効な選択です。\n");
				pause_continue(1);
		}
	}
}

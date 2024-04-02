#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

void traverseDirectory(const char *basePath, int outputFile) {
    DIR *dir = opendir(basePath);
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    struct dirent *entry;
    struct stat fileStat;

    while ((entry = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);

        if (stat(path, &fileStat) < 0) {
            perror("Error getting file stat");
            closedir(dir);
            return;
        }

        if (S_ISDIR(fileStat.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            traverseDirectory(path, outputFile);
        }

        struct passwd *pwd = getpwuid(fileStat.st_uid);
        struct group *grp = getgrgid(fileStat.st_gid);
        char permissions[11];
        permissions[0] = (S_ISDIR(fileStat.st_mode)) ? 'd' : '-';
        permissions[1] = (fileStat.st_mode & S_IRUSR) ? 'r' : '-';
        permissions[2] = (fileStat.st_mode & S_IWUSR) ? 'w' : '-';
        permissions[3] = (fileStat.st_mode & S_IXUSR) ? 'x' : '-';
        permissions[4] = (fileStat.st_mode & S_IRGRP) ? 'r' : '-';
        permissions[5] = (fileStat.st_mode & S_IWGRP) ? 'w' : '-';
        permissions[6] = (fileStat.st_mode & S_IXGRP) ? 'x' : '-';
        permissions[7] = (fileStat.st_mode & S_IROTH) ? 'r' : '-';
        permissions[8] = (fileStat.st_mode & S_IWOTH) ? 'w' : '-';
        permissions[9] = (fileStat.st_mode & S_IXOTH) ? 'x' : '-';
        permissions[10] = '\0';

        char lastModified[20];
        strftime(lastModified, sizeof(lastModified), "%Y-%m-%d %H:%M:%S", localtime(&fileStat.st_mtime));

        char buffer[2048];
        snprintf(buffer, sizeof(buffer), "File: %s\n"
                                            "Permissions: %s\n"
                                            "Owner: %s\n"
                                            "Group: %s\n"
                                            "Size: %ld bytes\n"
                                            "Last Modified: %s\n"
                                            "------------------------------------------\n",
                 path, permissions, (pwd != NULL) ? pwd->pw_name : "", (grp != NULL) ? grp->gr_name : "", fileStat.st_size, lastModified);
        write(outputFile, buffer, strlen(buffer));
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s Directory_Path\n", argv[0]);
        return 1;
    }

    int outputFile = open("OutputFile", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (outputFile < 0) {
        perror("Error opening output file");
        return 1;
    }

    traverseDirectory(argv[1], outputFile);

    close(outputFile);

    return 0;
}

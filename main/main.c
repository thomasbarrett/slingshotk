#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "blob.h"
#include "util.h"

typedef struct command {
    const char *name;
    const char *brief;

    // The parent command.
    struct command *parent;

    /**
     * Run the command.
     * 
     * @param cmd the command
     * @param argc the argument count
     * @param argv the argument vector
     */
    int (*run)(struct command *cmd, int argc, char const *argv[]);
} command_t;

int blobstore_create_func(command_t *cmd, int argc, char const *argv[]) {

    int fd = open("/dev/nvme0n1", O_RDWR | O_DIRECT);
    if (fd < 0) {
        perror("failed to open block device");
        exit(1);
    }

    blobstore_t bs;
    blobstore_init(&bs, fd);
    
    close(fd);

    blobstore_deinit(&bs);

    return 0;
}

int blob_create_func(command_t *cmd, int argc, char const *argv[]) {
    if (argc != 2) return -1;

    uint64_t n_clusters;
    if (parse_u64(argv[1], &n_clusters) < 0) return -1;

    int fd = open("/dev/nvme0n1", O_RDWR | O_DIRECT);
    if (fd < 0) {
        perror("failed to open block device");
        exit(1);
    }

    blobstore_t bs;
    blobstore_open(&bs, fd);
    
    if (blobstore_create_blob(&bs, n_clusters) < 0) {
        perror("failed to create blob");
        exit(1);
    }

    close(fd);

    printf("blob created\n");

    blobstore_deinit(&bs);

    return 0;
}


int blob_delete_func(command_t *cmd, int argc, char const *argv[]) {
  
    int fd = open("/dev/nvme0n1", O_RDWR | O_DIRECT);
    if (fd < 0) {
        perror("failed to open block device");
        exit(1);
    }

    blobstore_t bs;
    blobstore_open(&bs, fd);
    
    if (blobstore_delete_blob(&bs, bs.head) < 0) {
        perror("failed to delete blob");
        exit(1);
    }

    close(fd);

    printf("blob deleted\n");

    blobstore_deinit(&bs);

    return 0;
}

int blobstore_list_func(command_t *cmd, int argc, char const *argv[]) {
    int fd = open("/dev/nvme0n1", O_RDWR | O_DIRECT);
    if (fd < 0) {
        perror("failed to open block device");
        exit(1);
    }

    blobstore_t bs;
    blobstore_open(&bs, fd);
    
    printf("page size:\t%08llx\n", 1ULL << bs.page_shift);
    printf("cluster size:\t%08llx\n", 1ULL << bs.page_shift << bs.cluster_shift);
    printf("metadata size:\t%08llx\n", 1ULL << bs.page_shift << bs.cluster_shift << bs.md_shift);
    printf("clusters:\t%08lx\n", bitset_capacity(&bs.clusters));

    blob_t *curr = bs.head;
    while (curr) {
        uuid_print(curr->uuid);

        bitset_t nonzero = {0};
        blob_nonzero(curr, &nonzero);
        uint32_t used = bitset_size(&nonzero) * 100 / bitset_capacity(&nonzero);
        bitset_deinit(&nonzero);
        
        printf(" 0x%04x 0x%08lx %d%%\n", curr->page_index, array_size(&curr->clusters), used);
        curr = curr->next;
    }

    close(fd);

    blobstore_deinit(&bs);

    return 0;
}

void cmd_print(command_t *cmd) {
    if (cmd) {
        cmd_print(cmd->parent);
        printf("%s ", cmd->name);
    }
}

void cmd_subcommand_help(command_t *cmd, command_t *subcmds[], size_t subcmds_len) {
    printf("Usage: ");
    cmd_print(cmd);
    printf("COMMAND\n\n");
    printf("Commands:\n");
    for (int i = 0; i < subcmds_len; i++) {
        command_t *subcmd = subcmds[i];
        const int col_width = 16;
        printf("   %s%*s%s\n", subcmd->name, (int) (col_width - strlen(subcmd->name)), "", subcmd->brief);
    }
}

int blob_cmd_func(command_t *cmd, int argc, char const *argv[]) {
    command_t create_cmd = {0};
    create_cmd.parent = cmd;
    create_cmd.name = "create";
    create_cmd.brief = "create a blob.";
    create_cmd.run = blob_create_func;

    command_t delete_cmd = {0};
    delete_cmd.parent = cmd;
    delete_cmd.name = "delete";
    delete_cmd.brief = "delete a blob.";
    delete_cmd.run = blob_delete_func;

    command_t *subcmds[] = {&create_cmd, &delete_cmd};
    if (argc == 1) goto error;

    for (int i = 0; i < (sizeof(subcmds) / sizeof(command_t*)); i++) {
        command_t *subcmd = subcmds[i];
        if (strcmp(subcmd->name, argv[1]) == 0) {
            return subcmd->run(subcmd, argc - 1, &argv[1]);
        }
    }

error:
    cmd_subcommand_help(cmd, subcmds, (sizeof(subcmds) / sizeof(command_t*)));
    return -1;
}

int blobstore_cmd_func(command_t *cmd, int argc, char const *argv[]) {
    command_t create_cmd = {0};
    create_cmd.parent = cmd;
    create_cmd.name = "create";
    create_cmd.brief = "create a blobstore.";
    create_cmd.run = blobstore_create_func;

    command_t list_cmd = {0};
    list_cmd.parent = cmd;
    list_cmd.name = "list";
    list_cmd.brief = "list all blobs.";
    list_cmd.run = blobstore_list_func;

    command_t *subcmds[] = {&create_cmd, &list_cmd};
    if (argc == 1) goto error;

    for (int i = 0; i < (sizeof(subcmds) / sizeof(command_t*)); i++) {
        command_t *subcmd = subcmds[i];
        if (strcmp(subcmd->name, argv[1]) == 0) {
            return subcmd->run(subcmd, argc - 1, &argv[1]);
        }
    }

error:
    cmd_subcommand_help(cmd, subcmds, (sizeof(subcmds) / sizeof(command_t*)));
    return -1;
}

int root_cmd_func(command_t *cmd, int argc, char const *argv[]) {
    command_t blobstore_cmd = {0};
    blobstore_cmd.parent = cmd;
    blobstore_cmd.name = "blobstore";
    blobstore_cmd.brief = "manage blobstore.";
    blobstore_cmd.run = blobstore_cmd_func;

    command_t blob_cmd = {0};
    blob_cmd.parent = cmd;
    blob_cmd.name = "blob";
    blob_cmd.brief = "manage blob.";
    blob_cmd.run = blob_cmd_func;

    command_t *subcmds[] = {&blobstore_cmd, &blob_cmd};
    if (argc == 1) goto error;

    for (int i = 0; i < (sizeof(subcmds) / sizeof(command_t*)); i++) {
        command_t *subcmd = subcmds[i];
        if (strcmp(subcmd->name, argv[1]) == 0) {
            return subcmd->run(subcmd, argc - 1, &argv[1]);
        }
    }

error:
    cmd_subcommand_help(cmd, subcmds, (sizeof(subcmds) / sizeof(command_t*)));
    return -1;
}

int main(int argc, char const *argv[]) {
    command_t root_cmd = {0};
    root_cmd.name = argv[0];
    root_cmd.run = root_cmd_func;
    return root_cmd.run(&root_cmd, argc, argv);
}

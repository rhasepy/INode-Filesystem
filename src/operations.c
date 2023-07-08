#include "../lib/operations.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to find the directory inode index given the path
int find_parent_directory(file_system* fs, char* path) {
    // Separate the path into directory names
    char* token = strtok(path, "/");
    int parent_inode_num = fs->root_node;

    // Traverse the path to find the parent directory
    while (token != NULL) {
        int found = 0;
        for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
            int inode_num = fs->inodes[parent_inode_num].direct_blocks[i];
            if (inode_num != -1) {
                inode* curr_inode = &fs->inodes[inode_num];
                if (strcmp(curr_inode->name, token) == 0) {
                    if (curr_inode->n_type == directory) {
                        parent_inode_num = inode_num;
                        found = 1;
                        break;
                    } else {
                        printf("'%s' is not a directory.\n", token);
                        return -1;
                    }
                }
            }
        }

        if (!found) {
            printf("Directory '%s' not found.\n", token);
            return -1;
        }

        token = strtok(NULL, "/");
    }

    return parent_inode_num;
}

char*
reverse_str(char *str)
{
    char *p1, *p2;

    if (! str || ! *str)
        return str;

    for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2) {
        *p1 ^= *p2;
        *p2 ^= *p1;
        *p1 ^= *p2;
    }
    return str;
}

int
find_free_data_block(file_system* fs) {
    // Iterate over data blocks and find the first free data block
    for (int i = 0; i < fs->s_block->num_blocks; i++) {
        if (fs->free_list[i]) {
            fs->free_list[i] = 0; // Mark the data block as used
            fs->s_block->free_blocks--;
            return i;
        }
    }
    return -1; // No free data block found
}

int
fs_mkdir(file_system* fs, char* path) {
    // Separate the parent directory and the new directory name
    char* parent_path = NULL;
    char* dir_name = path;

    // Find the last separator '/' in the path
    char* separator = strrchr(path, '/');
    if (separator != NULL) {
        *separator = '\0'; // Replace '/' with null character
        parent_path = path;
        dir_name = separator + 1; // Get the directory name after the separator
    }

    // Find the parent directory or assume root as the parent if the parent_path is NULL
    inode* parent_dir = &fs->inodes[fs->root_node];
    if (parent_path != NULL) {
        char* token = strtok(parent_path, "/");
        while (token != NULL) {
            int found = 0;
            for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
                int inode_num = parent_dir->direct_blocks[i];
                if (inode_num != -1) {
                    inode* curr_inode = &fs->inodes[inode_num];
                    if (strcmp(curr_inode->name, token) == 0 && curr_inode->n_type == directory) {
                        parent_dir = curr_inode;
                        found = 1;
                        break;
                    }
                }
            }
            if (!found) {
                printf("Parent directory '%s' not found.\n", token);
                return -1;
            }
            token = strtok(NULL, "/");
        }
    }

    // Check if the directory already exists in the parent directory
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        int inode_num = parent_dir->direct_blocks[i];
        if (inode_num != -1) {
            inode* curr_inode = &fs->inodes[inode_num];
            if (strcmp(curr_inode->name, dir_name) == 0 && curr_inode->n_type == directory) {
                printf("Directory '%s' already exists.\n", dir_name);
                return -1;
            }
        }
    }

    // Find a free inode for the new directory
    int new_inode_num = -1;
    for (int i = 0; i < fs->s_block->num_blocks; i++) {
        if (fs->free_list[i] == 1) {
            new_inode_num = i;
            fs->free_list[i] = 0;
            break;
        }
    }
    if (new_inode_num == -1) {
        printf("No free inode available.\n");
        return -1;
    }

    // Create the new directory inode
    inode* new_dir = &fs->inodes[new_inode_num];
    new_dir->n_type = directory;
    new_dir->size = 0;
    strcpy(new_dir->name, dir_name);
    new_dir->parent = parent_dir - fs->inodes; // Calculate parent inode number

    // Update the parent directory entry
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        if (parent_dir->direct_blocks[i] == -1) {
            parent_dir->direct_blocks[i] = new_inode_num;
            break;
        }
    }

    printf("Directory '%s' created successfully.\n", new_dir->name);
    return 0;
}

int
fs_mkfile(file_system* fs, char* path_and_name) {
    // Find the last occurrence of '/' to separate the path and filename
    char* last_slash = strrchr(path_and_name, '/');
    if (last_slash == NULL) {
        // No path specified, create the file in the root directory
        char* filename = path_and_name;

        // Check if the file already exists in the root directory
        for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
            int inode_num = fs->inodes[fs->root_node].direct_blocks[i];
            if (inode_num != -1) {
                inode* curr_inode = &fs->inodes[inode_num];
                if (strcmp(curr_inode->name, filename) == 0 && curr_inode->n_type == reg_file) {
                    printf("File '%s' already exists.\n", filename);
                    return -2;
                }
            }
        }

        // Find a free inode for the new file
        int new_inode_num = find_free_inode(fs);
        if (new_inode_num == -1) {
            printf("No free inode available.\n");
            return -1;
        }

        // Create the new file inode in the root directory
        inode* new_file = &fs->inodes[new_inode_num];
        new_file->n_type = reg_file;
        new_file->size = 0;
        strcpy(new_file->name, filename);
        new_file->parent = fs->root_node;

        // Update the root directory entry
        for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
            if (fs->inodes[fs->root_node].direct_blocks[i] == -1) {
                fs->inodes[fs->root_node].direct_blocks[i] = new_inode_num;
                break;
            }
        }

        printf("File '%s' created successfully in the root directory.\n", filename);
        return 0;
    }

    // Separate the path and filename
    *last_slash = '\0';  // Replace the last '/' with '\0' to separate the path
    char* path = path_and_name;
    char* filename = last_slash + 1;

    // Find the parent directory or create it if it doesn't exist
    inode* parent_dir = &fs->inodes[fs->root_node];
    char* token = strtok(path, "/");
    while (token != NULL) {
        int found = 0;
        for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
            int inode_num = parent_dir->direct_blocks[i];
            if (inode_num != -1) {
                inode* curr_inode = &fs->inodes[inode_num];
                if (strcmp(curr_inode->name, token) == 0 && curr_inode->n_type == directory) {
                    parent_dir = curr_inode;
                    found = 1;
                    break;
                }
            }
        }
        if (!found) {
            // Parent directory does not exist
            printf("Parent directory '%s' does not exist.\n", token);
            return -1;
        }
        token = strtok(NULL, "/");
    }

    // Check if the file already exists in the parent directory
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        int inode_num = parent_dir->direct_blocks[i];
        if (inode_num != -1) {
            inode* curr_inode = &fs->inodes[inode_num];
            if (strcmp(curr_inode->name, filename) == 0 && curr_inode->n_type == reg_file) {
                printf("File '%s' already exists.\n", filename);
                return -2;
            }
        }
    }

    // Find a free inode for the new file
    int new_inode_num = find_free_inode(fs);
    if (new_inode_num == -1) {
        printf("No free inode available.\n");
        return -1;
    }

    // Create the new file inode
    inode* new_file = &fs->inodes[new_inode_num];
    new_file->n_type = reg_file;
    new_file->size = 0;
    strcpy(new_file->name, filename);
    new_file->parent = parent_dir - fs->inodes;

    // Update the parent directory entry
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        if (parent_dir->direct_blocks[i] == -1) {
            parent_dir->direct_blocks[i] = new_inode_num;
            break;
        }
    }

    printf("File '%s' created successfully.\n", filename);
    return 0;
}

char*
fs_list(file_system* fs, char* path) {
    // Find the directory specified by the path
    inode* curr_dir = &fs->inodes[fs->root_node];
    char* token = strtok(path, "/");
    while (token != NULL) {
        int found = 0;
        for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
            int inode_num = curr_dir->direct_blocks[i];
            if (inode_num != -1) {
                inode* next_dir = &fs->inodes[inode_num];
                if (strcmp(next_dir->name, token) == 0 && next_dir->n_type == directory) {
                    curr_dir = next_dir;
                    found = 1;
                    break;
                }
            }
        }
        if (!found) {
            printf("Directory '%s' not found.\n", token);
            return NULL;
        }
        token = strtok(NULL, "/");
    }

    // Count the number of entries in the directory
    int num_entries = 0;
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        int inode_num = curr_dir->direct_blocks[i];
        if (inode_num != -1) {
            inode* entry_inode = &fs->inodes[inode_num];
            if (entry_inode->n_type == directory || entry_inode->n_type == reg_file) {
                num_entries++;
            }
        }
    }

    // Allocate memory for the result string
    int result_size = num_entries * (NAME_MAX_LENGTH + 10); // Assuming max length of entry name + 10 characters for "DIR " or "FILE "
    char* result = (char*)malloc((result_size + 1) * sizeof(char));
    result[0] = '\0';

    // Concatenate the directory entries to the result string
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        int inode_num = curr_dir->direct_blocks[i];
        if (inode_num != -1) {
            inode* entry_inode = &fs->inodes[inode_num];
            if (entry_inode->n_type == directory) {
                snprintf(result + strlen(result), result_size - strlen(result), "DIR %s\n", entry_inode->name);
            } else if (entry_inode->n_type == reg_file) {
                snprintf(result + strlen(result), result_size - strlen(result), "FILE %s\n", entry_inode->name);
            }
        }
    }

    return result;
}

int fs_writef(file_system* fs, char* filepath, char* text) {
    // Separate the path and filename
    char* path = strdup(filepath);
    char* filename = strrchr(path, '/');
    if (filename != NULL) {
        *filename = '\0';  // Terminate the path string at the last '/'
        filename++;  // Skip the '/' character
    } else {
        // No directory path specified, use the root directory
        filename = path;
        path = NULL;
    }

    // Find the parent directory inode
    int parent_inode_num;
    if (path == NULL) {
        parent_inode_num = fs->root_node;
    } else {
        parent_inode_num = find_parent_directory(fs, path);
        if (parent_inode_num == -1) {
            printf("Directory '%s' not found.\n", path);
            free(path);
            return -1;
        }
    }

    // Find the inode of the file
    int file_inode_num = -1;
    inode* parent_inode = &fs->inodes[parent_inode_num];
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        int inode_num = parent_inode->direct_blocks[i];
        if (inode_num != -1) {
            inode* curr_inode = &fs->inodes[inode_num];
            if (strcmp(curr_inode->name, filename) == 0) {
                file_inode_num = inode_num;
                break;
            }
        }
    }

    if (file_inode_num == -1) {
        printf("File '%s' does not exist.\n", filepath);
        free(path);
        return -1;
    }

    inode* file_inode = &fs->inodes[file_inode_num];
    if (file_inode->n_type != reg_file) {
        printf("'%s' is not a regular file.\n", filepath);
        return -1;
    }

    // Find the last used data block index
    int last_block_idx = -1;
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        if (file_inode->direct_blocks[i] == -1) {
            last_block_idx = i - 1;
            break;
        }
    }

    // Calculate the total size of the text to be appended
    int total_text_len = strlen(text);
    int chars_written = 0;

    // Iterate over the existing data blocks
    for (int i = 0; i <= last_block_idx; i++) {
        int block_num = file_inode->direct_blocks[i];
        data_block* block = &fs->data_blocks[block_num];
        int remaining_space = BLOCK_SIZE - block->size;

        if (remaining_space > 0) {
            // There is remaining space in the block
            int text_len = (total_text_len > remaining_space) ? remaining_space : total_text_len;

            memcpy(block->block + block->size, text, text_len);
            block->size += text_len;
            chars_written += text_len;

            text += text_len;
            total_text_len -= text_len;

            if (total_text_len == 0) {
                // All text has been written
                return chars_written;
            }
        }
    }

    // If there is more text to be written, allocate new data blocks
    while (total_text_len > 0) {
        // Find a free data block
        int new_block_num = find_free_data_block(fs);
        if (new_block_num == -1) {
            printf("File '%s' is full.\n", filepath);
            return -2;
        }

        // Update the file inode with the new data block
        if (last_block_idx == DIRECT_BLOCKS_COUNT - 1) {
            printf("File '%s' is full.\n", filepath);
            return -2;
        }

        last_block_idx++;
        file_inode->direct_blocks[last_block_idx] = new_block_num;

        // Write as much text as possible to the new block
        data_block* new_block = &fs->data_blocks[new_block_num];
        int remaining_space = BLOCK_SIZE;
        int copy_len = (total_text_len > remaining_space) ? remaining_space : total_text_len;

        memcpy(new_block->block, text, copy_len);
        new_block->size = copy_len;
        chars_written += copy_len;

        text += copy_len;
        total_text_len -= copy_len;
    }

    return chars_written;
}

uint8_t*
fs_readf(file_system* fs, char* filepath, int* file_size) {
    // Separate the path and filename
    char* path = strdup(filepath);
    char* filename = strrchr(path, '/');
    if (filename != NULL) {
        *filename = '\0';  // Terminate the path string at the last '/'
        filename++;  // Skip the '/' character
    } else {
        // No directory path specified, use the root directory
        filename = path;
        path = NULL;
    }

    // Find the parent directory inode
    int parent_inode_num;
    if (path == NULL) {
        parent_inode_num = fs->root_node;
    } else {
        parent_inode_num = find_parent_directory(fs, path);
        if (parent_inode_num == -1) {
            printf("Directory '%s' not found.\n", path);
            free(path);
            return NULL;
        }
    }

    // Find the inode of the file
    int file_inode_num = -1;
    inode* parent_inode = &fs->inodes[parent_inode_num];
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        int inode_num = parent_inode->direct_blocks[i];
        if (inode_num != -1) {
            inode* curr_inode = &fs->inodes[inode_num];
            if (strcmp(curr_inode->name, filename) == 0) {
                file_inode_num = inode_num;
                break;
            }
        }
    }

    if (file_inode_num == -1) {
        printf("File '%s' does not exist.\n", filepath);
        free(path);
        return NULL;
    }

    inode* file_inode = &fs->inodes[file_inode_num];
    if (file_inode->n_type != reg_file) {
        printf("'%s' is not a regular file.\n", filename);
        return NULL;
    }

    // Calculate the file size
    *file_size = 0;
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        int block_num = file_inode->direct_blocks[i];
        if (block_num != -1) {
            data_block* block = &fs->data_blocks[block_num];
            *file_size += block->size;
        }
    }

    // Allocate memory for the buffer
    uint8_t* buffer = (uint8_t*)malloc(*file_size);
    if (buffer == NULL) {
        printf("Memory allocation failed.\n");
        return NULL;
    }

    // Read the file into the buffer
    uint8_t* ptr = buffer;
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        int block_num = file_inode->direct_blocks[i];
        if (block_num != -1) {
            data_block* block = &fs->data_blocks[block_num];
            memcpy(ptr, block->block, block->size);
            ptr += block->size;
        }
    }

    return buffer;
}

int
fs_rm(file_system* fs, char* path) {
    return 0;
}

int
fs_import(file_system* fs, char* int_path, char* ext_path) {
    // Open the external file for reading
    FILE* file = fopen(ext_path, "rb");
    if (file == NULL) {
        printf("Failed to open external file.\n");
        return -1;
    }

    // Get the parent directory inode number
    int parent_inode_number = fs->root_node;
    char* token = strtok(int_path, "/");
    while (token != NULL) {
        // Check if the parent inode is a directory
        if (fs->inodes[parent_inode_number].n_type != directory) {
            printf("Parent is not a directory.\n");
            fclose(file);
            return -1;
        }

        // Check if the directory exists
        int child_inode_number = -1;
        for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
            if (fs->inodes[parent_inode_number].direct_blocks[i] != -1 &&
                strcmp(fs->inodes[fs->inodes[parent_inode_number].direct_blocks[i]].name, token) == 0) {
                child_inode_number = fs->inodes[parent_inode_number].direct_blocks[i];
                break;
            }
        }

        // Directory does not exist
        if (child_inode_number == -1) {
            printf("Directory does not exist.\n");
            fclose(file);
            return -1;
        }

        parent_inode_number = child_inode_number;
        token = strtok(NULL, "/");
    }

    // Check if the file already exists in the parent directory
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        if (fs->inodes[parent_inode_number].direct_blocks[i] != -1 &&
            strcmp(fs->inodes[fs->inodes[parent_inode_number].direct_blocks[i]].name, ext_path) == 0) {
            printf("File already exists.\n");
            fclose(file);
            return -1;
        }
    }

    // Find a free inode for the new file
    int new_inode_number = find_free_inode(fs);
    if (new_inode_number == -1) {
        printf("No free inode available.\n");
        fclose(file);
        return -1;
    }

    // Find a free data block for the new file
    int new_data_block_number = find_free_data_block(fs);
    if (new_data_block_number == -1) {
        printf("No free data block available.\n");
        fclose(file);
        return -1;
    }

    // Update the parent inode with the new file entry
    fs->inodes[parent_inode_number].direct_blocks[fs->inodes[parent_inode_number].size] = new_inode_number;
    fs->inodes[parent_inode_number].size++;
    strcpy(fs->inodes[new_inode_number].name, ext_path);
    fs->inodes[new_inode_number].n_type = reg_file;
    fs->inodes[new_inode_number].size = 0;
    fs->inodes[new_inode_number].parent = parent_inode_number;
    fs->inodes[new_inode_number].direct_blocks[0] = new_data_block_number;

    // Read the external file and write its contents to the data block
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);
    fread(fs->data_blocks[new_data_block_number].block, 1, file_size, file);
    fs->data_blocks[new_data_block_number].size = file_size;

    // Update the free blocks count
    fs->s_block->free_blocks -= (file_size / BLOCK_SIZE) + 1;

    fclose(file);
    printf("File imported successfully.\n");
    return 0;
}

int
fs_export(file_system* fs, char* int_path, char* ext_path) {
    // Get the parent directory inode number
    int parent_inode_number = fs->root_node;
    char* token = strtok(int_path, "/");
    while (token != NULL) {
        // Check if the parent inode is a directory
        if (fs->inodes[parent_inode_number].n_type != directory) {
            printf("Parent is not a directory.\n");
            return -1;
        }

        // Check if the directory exists
        int child_inode_number = -1;
        for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
            if (fs->inodes[parent_inode_number].direct_blocks[i] != -1 &&
                strcmp(fs->inodes[fs->inodes[parent_inode_number].direct_blocks[i]].name, token) == 0) {
                child_inode_number = fs->inodes[parent_inode_number].direct_blocks[i];
                break;
            }
        }

        // Directory does not exist
        if (child_inode_number == -1) {
            printf("Directory does not exist.\n");
            return -1;
        }

        parent_inode_number = child_inode_number;
        token = strtok(NULL, "/");
    }

    // Check if the file exists in the parent directory
    int file_inode_number = -1;
    for (int i = 0; i < DIRECT_BLOCKS_COUNT; i++) {
        if (fs->inodes[parent_inode_number].direct_blocks[i] != -1 &&
            strcmp(fs->inodes[fs->inodes[parent_inode_number].direct_blocks[i]].name, ext_path) == 0) {
            file_inode_number = fs->inodes[parent_inode_number].direct_blocks[i];
            break;
        }
    }

    // File does not exist
    if (file_inode_number == -1) {
        printf("File does not exist.\n");
        return -1;
    }

    // Get the data block number for the file
    int data_block_number = fs->inodes[file_inode_number].direct_blocks[0];

    // Check if the data block exists
    if (data_block_number == -1) {
        printf("Data block does not exist.\n");
        return -1;
    }

    // Open the external file for writing
    FILE* file = fopen(ext_path, "wb");
    if (file == NULL) {
        printf("Failed to open external file.\n");
        return -1;
    }

    // Write the file contents from the data block to the external file
    fwrite(fs->data_blocks[data_block_number].block, 1, fs->data_blocks[data_block_number].size, file);

    fclose(file);
    printf("File exported successfully.\n");
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE 1024

#define PARSEMODE_FIND_PID_END 0
#define PARSEMODE_FIND_PARENT_PID_END 1
#define PARSEMODE_FIND_NAME_END 2
#define PARSEMODE_FIND_STATUS_END 3
#define PARSEMODE_FIND_LINE_END 4

#define PARSESTATUS_SEEKING_START 0
#define PARSESTATUS_SEEKING_END 1

int is_line_separator(int x) {
    return x == '\n' || x == '\0';
}


int is_delimiter(int x) {
    return x == ' ' || x == '\t' || is_line_separator(x);
}


int inactive_status(int code) {
    return code == 'T' || code == 'X' || code == 'Z';
}


int parse_integer(const char* string, size_t start, size_t end) {
    int output = 0;

    string += start;
    for (; start++ <= end; string++) {
        output *= 10;
        output += *string - '0';
    }

    return output;
}


void trim_string(const char* string, size_t* begin, size_t* end) {
    size_t _begin, _end;

    for (_begin = *begin; is_delimiter(string[_begin]); _begin++);
    for (_end = *end; is_delimiter(string[_end]); _end--);

    *begin = _begin;
    *end = _end;
}


void find_item_ends(
    const char* output,
    size_t* pid_end,
    size_t* parent_pid_end,
    size_t* name_end,
    size_t* status_end)
{
    size_t position;
    int parse_mode = PARSEMODE_FIND_PID_END,
        parse_status = PARSESTATUS_SEEKING_START;

    // Each column is right aligned, except the tty, which is left aligned
    // and columns go in order pid, parent pid, name, status, and tty

    for (position = 0; !is_line_separator(output[position]); position++) {
        // since the name may include column delimiters
        // there is no way to determine where the name ends in this loop
        //
        // because of this we only find the end of the pids in this loop

        if (parse_mode == PARSEMODE_FIND_PID_END || parse_mode == PARSEMODE_FIND_PARENT_PID_END) {

            if (parse_status == PARSESTATUS_SEEKING_START && !is_delimiter(output[position]))
                parse_status = PARSESTATUS_SEEKING_END;
            else if (parse_status == PARSESTATUS_SEEKING_END && is_delimiter(output[position])) {
                parse_status = PARSESTATUS_SEEKING_START;

                if (parse_mode == PARSEMODE_FIND_PID_END) {
                    *pid_end = position - 1;
                    parse_mode = PARSEMODE_FIND_PARENT_PID_END;
                } else {
                    *parent_pid_end = position - 1;
                    parse_mode = PARSEMODE_FIND_LINE_END;
                }
            }
        }
    }

    // The first entry is pid1, which can't have a tty yet
    // It is indicated by ?, and so we have [STATUS] ?\n
    // so we subtract out 3 characters to obtain the status end offset
    *status_end = position - 3;

    // Statuses are always 1 character, and so
    // we can subtract 2 more characters to find the end of the name
    *name_end = *status_end - 2;
}


void parse_output(
    const char* output,
    size_t* lines,
    int** pids,
    int** parent_pids,
    const char*** names,
    size_t** name_lengths,
    char** statuses,
    const char*** ttys,
    size_t** tty_lengths)
{
    size_t pid_begin_offset, pid_end_offset,
           parent_pid_begin_offset, parent_pid_end_offset,
           name_begin_offset, name_end_offset,
           status_begin_offset, status_end_offset,
           tty_begin_offset,
           line_count, item_count,
           current_position;
    const char** _names = NULL,
              ** _ttys = NULL;
    char* _statuses = NULL;
    int* _pids = NULL,
       * _parent_pids = NULL;
    size_t* _name_lengths = NULL,
          * _tty_lengths = NULL;

    find_item_ends(output, &pid_end_offset, &parent_pid_end_offset, &name_end_offset, &status_end_offset);

    pid_begin_offset = 0;
    parent_pid_begin_offset = pid_end_offset + 2;
    name_begin_offset = parent_pid_end_offset + 2;
    status_begin_offset = name_end_offset + 2;
    tty_begin_offset = status_end_offset + 2;

    for (current_position = 0, line_count = 0, item_count = 0; output[current_position]; line_count++) {
        if (line_count >= item_count) {
            item_count += CHUNK_SIZE;

            _pids = realloc(_pids, item_count * sizeof(*_pids));
            _parent_pids = realloc(_parent_pids, item_count * sizeof(*_parent_pids));
            _names = realloc(_names, item_count * sizeof(*_names));
            _statuses = realloc(_statuses, item_count * sizeof(*_statuses));
            _ttys = realloc(_ttys, item_count * sizeof(*_ttys));
            _name_lengths = realloc(_name_lengths, item_count * sizeof(*_name_lengths));
            _tty_lengths = realloc(_tty_lengths, item_count * sizeof(*_tty_lengths));
        }

        size_t start, end;

        start = current_position + pid_begin_offset;
        end = current_position + pid_end_offset;
        trim_string(output, &start, &end);
        _pids[line_count] = parse_integer(output, start, end);

        start = current_position + parent_pid_begin_offset;
        end = current_position + parent_pid_end_offset;
        trim_string(output, &start, &end);
        _parent_pids[line_count] = parse_integer(output, start, end);

        start = current_position + name_begin_offset;
        end = current_position + name_end_offset;
        trim_string(output, &start, &end);
        _names[line_count] = &output[start];
        _name_lengths[line_count] = end - start + 1;

        start = current_position + status_begin_offset;
        _statuses[line_count] = output[start];

        start = current_position + tty_begin_offset;
        end = strchr(&output[start], '\n') - output;
        _ttys[line_count] = &output[start];
        _tty_lengths[line_count] = end - start;

        current_position = end + 1;
    }

    *lines = line_count;
    *pids = _pids;
    *parent_pids = _parent_pids;
    *names = _names;
    *name_lengths = _name_lengths;
    *statuses = _statuses;
    *ttys = _ttys;
    *tty_lengths = _tty_lengths;
}


struct cleanup_information {
    FILE* handle;
    char* buffer,
        * statuses;
    int* pids,
       * parent_pids;
    size_t * current_proc_indices,
           * next_proc_indices,
           * name_lengths,
           * tty_lengths;
} cleanup_info;


void cleanup_resources() {
    // basic cleanup function
    // non-null handles have been initialized and should be cleaned up

    if (cleanup_info.handle)
        pclose(cleanup_info.handle);
    if (cleanup_info.buffer)
        free(cleanup_info.buffer);
    if (cleanup_info.statuses)
        free(cleanup_info.statuses);
    if (cleanup_info.pids)
        free(cleanup_info.pids);
    if (cleanup_info.parent_pids)
        free(cleanup_info.parent_pids);
    if (cleanup_info.current_proc_indices)
        free(cleanup_info.current_proc_indices);
    if (cleanup_info.next_proc_indices)
        free(cleanup_info.next_proc_indices);
    if (cleanup_info.name_lengths)
        free(cleanup_info.name_lengths);
    if (cleanup_info.tty_lengths)
        free(cleanup_info.tty_lengths);
}


int main(int argc, char ** argv) {
    FILE* handle = NULL;
    char* buffer = NULL;
    size_t buffer_size, read_size;
    int* pids = NULL,
       * parent_pids = NULL,
       exit_code;
    char* statuses = NULL;
    const char** names = NULL,
              ** ttys = NULL;

    size_t lines,
           current_proc_count,
           next_proc_count,
           * current_proc_indices = NULL,
           * next_proc_indices = NULL,
           * name_lengths = NULL,
           * tty_lengths = NULL;

    memset(&cleanup_info, 0, sizeof(cleanup_info));
    atexit(cleanup_resources);

    if (argc != 2) {
        fputs("No tty given!\n", stderr);
        return 1;
    } else if (argv[1][0] == '/') {
        // oftentimes the device of the tty will be passed instead of its name as given by ps
        // so we adjust the pointer of the tty name if needed

        const char* dev = "/dev/";

        if (strncmp(argv[1], dev, strlen(dev)) == 0)
            argv[1] += strlen(dev);
    }

    // since obtaining a list of open processes is platform dependent
    // we pipe the information from ps and parse the output
    handle = popen("ps ax -o pid=,ppid=,comm=,state=,tname=", "r");
    cleanup_info.handle = handle;

    if (!handle) {
        // failed to open ps
        perror("Failed to open ps");
        return 1;
    }

    buffer_size = 0;
    read_size = 0;

    while (!feof(handle)) {
        // realloc buffer for new data
        buffer = realloc(buffer, buffer_size + CHUNK_SIZE);
        cleanup_info.buffer = buffer;

        read_size = fread(buffer + buffer_size, 1, CHUNK_SIZE, handle);

        // adjust buffer size
        buffer_size += read_size;
    }

    // close handle and set it to NULL so cleanup doesn't try to close it
    pclose(handle);
    cleanup_info.handle = NULL;

    parse_output(buffer, &lines, &pids, &parent_pids, &names, &name_lengths, &statuses, &ttys, &tty_lengths);

    cleanup_info.pids = pids;
    cleanup_info.parent_pids = parent_pids;
    cleanup_info.name_lengths = name_lengths;
    cleanup_info.statuses = statuses;
    cleanup_info.tty_lengths = tty_lengths;

    current_proc_indices = malloc(lines * sizeof(*current_proc_indices));
    next_proc_indices = malloc(lines * sizeof(*next_proc_indices));

    cleanup_info.current_proc_indices = current_proc_indices;
    cleanup_info.next_proc_indices = next_proc_indices;

    // populate process list with processes that have the given tty
    current_proc_count = 0;
    for (size_t i = 0; i < lines; i++)
        if (strncmp(ttys[i], argv[1], tty_lengths[i]) == 0)
            current_proc_indices[current_proc_count++] = i;

    for (;;) {
        for (size_t i = 0; i < current_proc_count; i++) {
            size_t index = current_proc_indices[i];
            if ((strncmp(names[index], "nvim", name_lengths[index]) == 0 ||
                strncmp(names[index], "vim", name_lengths[index]) == 0) &&
                !inactive_status(statuses[index]))
                return 0;
        }

        // obtain child processes of current process generation
        next_proc_count = 0;
        for (size_t i = 0; i < current_proc_count; i++) {
            for (size_t j = 0; j < lines; j++) {
                size_t index = current_proc_indices[i];
                if (pids[index] == parent_pids[j]) {
                    next_proc_indices[next_proc_count++] = j;
                }
            }
        }

        // if there are no child processes we failed to find vim
        if (next_proc_count == 0)
            return 1;

        // update current processes with the next processes
        // and swap the buffers to avoid extra calls to malloc and free
        size_t* tmp_indices = current_proc_indices;
        current_proc_indices = next_proc_indices;
        next_proc_indices = tmp_indices;

        current_proc_count = next_proc_count;
    }
}

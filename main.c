#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
}InputBuffer;

//
typedef enum{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
}MetaCommandResult;

//
typedef enum {
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT
}PreprareResult;

typedef enum{
    STATEMENT_INSERT,
    STATEMENT_SELECT
}StatementType;

typedef struct{
    StatementType type;
}Statement;

//初始化一个新的buffer
InputBuffer* new_input_buffer()
{
    InputBuffer* input_buffer = (InputBuffer*)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

//跟sqlite一样打印一个 db>
void print_prompt()
{
    printf("db>");
}

//获取输入
void read_input(InputBuffer* input_buffer)
{
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);
    if(bytes_read < 0){
        printf("error reading input\n");
        exit(EXIT_FAILURE);
    }

    input_buffer->input_length = bytes_read -1;
    input_buffer->buffer[bytes_read-1] = 0;
}

//手动在堆上申请了内存，需要手动释放
void close_input_buffer(InputBuffer* input_buffer)
{
    free(input_buffer->buffer);
    free(input_buffer);
}

//
MetaCommandResult do_meta_command(InputBuffer* inputbuffer){
    if(strcmp(inputbuffer->buffer, ".exit") == 0){
        exit(EXIT_SUCCESS);
    }else{
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

//
PreprareResult prepare_statement(InputBuffer* inputbuffer, Statement* statement){
    if(strncmp(inputbuffer->buffer, "insert", 6) == 0){
        statement->type = STATEMENT_SELECT;//statement->type == STATEMENT_SELECT;该语句会显示type=很大的数
        return PREPARE_SUCCESS;
    }
    if (strcmp(inputbuffer->buffer, "select") == 0)
    {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

//
void execute_statement(Statement* statement){
    switch (statement->type)
    {
    case (STATEMENT_INSERT):
        printf("This is where we would do an insert.\n");
        break;
    case (STATEMENT_SELECT):
        printf("This is where we would do a select.\n");
        break;
    }
}


int main(int argc, char* argv[])
{
    InputBuffer *input_buffer = new_input_buffer();

    while (1)
    {
        print_prompt();
        read_input(input_buffer);

        // if(strcmp(input_buffer->buffer, ".exit") == 0)
        // {
        //     close_input_buffer(input_buffer);
        //     exit(EXIT_SUCCESS);
        // }
        // else
        // {
        //     printf("Unrecognized command '%s'\n",input_buffer->buffer);
        // }
        if(input_buffer->buffer[0] == '.'){
            switch (do_meta_command(input_buffer))
            {
            case META_COMMAND_SUCCESS:
                break;
            case META_COMMAND_UNRECOGNIZED_COMMAND:
                printf("Unrecognized command '%s'\n", input_buffer->buffer);
                continue;
            }
        }

        Statement statement;
        switch (prepare_statement(input_buffer, &statement))
        {
        case PREPARE_SUCCESS:
            break;

        case PREPARE_UNRECOGNIZED_STATEMENT:
            printf("Unrecognized keyword at start of '%s'.\n",input_buffer->buffer);
            break;
        default:
            break;
        }

        execute_statement(&statement);
        printf("Executed!\n");
    }//while（1）
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//常量定义
#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100


//表格的结构体
typedef struct {
    uint32_t num_rows;
    void* pages[TABLE_MAX_PAGES];
}Table;

//输入指令的结构体
typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
}InputBuffer;

//执行命令结果
typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_FULL
}ExecuteResult;

//元命令解析结果
typedef enum{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
}MetaCommandResult;

//查询及修改命令解析结果
typedef enum {
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,//第三章，新增解析格式错误
    PREPARE_UNRECOGNIZED_STATEMENT
}PrepareResult;

//解析具体结果，目前仅有两种，一个插入，一个选择
typedef enum{
    STATEMENT_INSERT,
    STATEMENT_SELECT
}StatementType;

typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];   
}Row;
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;
const uint32_t ROWS_PER_PAGES = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGES * TABLE_MAX_PAGES;

typedef struct{
    StatementType type;
    //第三章-增加statement接收参数
    Row row_to_insert;  //仅用于插入语句使用
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

//打印一行数据
void print_row(Row* row){
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
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

//创建新的表格
Table* new_table(){
    Table* table = malloc(sizeof(Table));
    table->num_rows = 0;
    for (uint32_t  i = 0; i < TABLE_MAX_PAGES; i++)
    {
        table->pages[i] = NULL;
    }
    
    return table;
}

//释放表格
void free_table(Table* table){
    for (int i = 0;table->pages[i]; i++)
    {
        free(table->pages[i]);
    }
    free(table);
}

//解析元命令的，例如相.exit退出命令这种
MetaCommandResult do_meta_command(InputBuffer* inputbuffer, Table* table){
    if(strcmp(inputbuffer->buffer, ".exit") == 0){
        close_input_buffer(inputbuffer);
        free_table(table);
        exit(EXIT_SUCCESS);
    }else{
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

//输入第一位不是.，那么就是向数据库查询或者修改的命令，此函数会将该命令打上具体的枚举，以便后续执行
PrepareResult prepare_statement(InputBuffer* inputbuffer, Statement* statement){
    if(strncmp(inputbuffer->buffer, "insert", 6) == 0){
        statement->type = STATEMENT_INSERT;
        //第三章，增加insert 接收更多参数
        int args_assigned = sscanf(
            inputbuffer->buffer,"insert %d %s %s", &(statement->row_to_insert.id),
            statement->row_to_insert.username,
            statement->row_to_insert.email
        );
        if(args_assigned < 3) return PREPARE_SYNTAX_ERROR;
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
void* row_slot(Table* table, uint32_t row_num){
    uint32_t page_num = row_num / ROWS_PER_PAGES;
    void* page = table->pages[page_num];
    if(page == NULL)
    {
        //如果该页为空，我们需要分配内存
        page = table->pages[page_num] = malloc(PAGE_SIZE);
    }
    uint32_t row_offset = row_num % ROWS_PER_PAGES;
    uint32_t byte_offset = row_offset * ROW_SIZE;

    return page + byte_offset;
}

//序列化一行
void serialize_row(Row* source, void* destination){
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

//反序列化一行
void deserialize_row(Row* source, Row* destination){
    memcpy(&(destination->id), &(source->id), ID_SIZE);
    memcpy(&(destination->username), &(source->username), USERNAME_SIZE);
    memcpy(&(destination->email), &(source->email), EMAIL_SIZE);
}


//insert函数
ExecuteResult execute_insert(Statement* statement, Table* table){
    if(table->num_rows >= TABLE_MAX_ROWS){
        return EXECUTE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);
    serialize_row(row_to_insert, row_slot(table, table->num_rows));
    table->num_rows += 1;

    return EXECUTE_SUCCESS;
}

//select函数
ExecuteResult execute_select(Statement* statement, Table* table){
    Row row;
    for(uint32_t i = 0;i<table->num_rows; i++){
        deserialize_row(row_slot(table, i), &row);
        print_row(&row);
    }
    return EXECUTE_SUCCESS;
}

//执行命令函数，后续大量工作在此函数中进行
ExecuteResult execute_statement(Statement* statement, Table* table){
    // printf("statement->type = %d",statement->type);
    switch (statement->type)
    {
    case (STATEMENT_INSERT):
        return execute_insert(statement, table);    
        //printf("This is where we would do an insert.\n");
        break;
    case (STATEMENT_SELECT):
        return execute_select(statement, table);
        // printf("This is where we would do a select.\n");
        break;
    }
}




int main(int argc, char* argv[])
{
    Table* table = new_table();
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
            switch (do_meta_command(input_buffer, table))
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

        case PREPARE_SYNTAX_ERROR:
            printf("Syntax error. Could not parse statement.\n");
            break;

        case PREPARE_UNRECOGNIZED_STATEMENT:
            printf("Unrecognized keyword at start of '%s'.\n",input_buffer->buffer);
            break;
        default:
            break;
        }

        switch(execute_statement(&statement, table)){
            case EXECUTE_SUCCESS:
                printf("Executed!\n");
            break;
            
            case EXECUTE_FULL:
                printf("Error: Table full.\n");
            break;
        }

    }//while（1）
}

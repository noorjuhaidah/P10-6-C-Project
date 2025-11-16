// Main.c — SUPER BEGINNER CMS (no 'static', interactive, file-friendly)
// Build:  gcc Main.c -o CMS
// Run:    ./CMS
//
// Features (match assignment table):
//  OPEN <filename>  -> open the database file and read in all records
//  SHOW ALL         -> display all current records in memory (supports sorting)
//  INSERT           -> if same ID exists: error+cancel; else PROMPT every column
//  QUERY ID=<n>     -> show record if found; else "no record found"
//  UPDATE ID=<n>    -> update record (Enter = keep current)
//  DELETE ID=<n>    -> delete record (double-confirm)
//  SAVE             -> save all current records back into the file
//  UNDO             -> revert the last INSERT, UPDATE, or DELETE operation   <-- Unique Feature
//  HELP / EXIT

// Unique Feature Implemented:
//  UNDO — This feature keeps an internal stack of the most recent modifying actions 
//          (INSERT, UPDATE, DELETE) and allows the user to revert the most recent change.
//          Demonstrates use of structs, arrays, state management, and integration with CMS logic.



#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* ---------- Simple configuration ---------- */
#define MAX_STUDENTS 1000
#define NAME_MAX_LEN 128
#define PROG_MAX_LEN 128
#define LINE_MAX_LEN 1024

/* ---------- Data type ---------- */
typedef struct {
    int   id;
    char  name[NAME_MAX_LEN];
    char  programme[PROG_MAX_LEN];
    float mark;
} Student;

/* ---------- UNDO FEATURE STRUCTURE ---------- */
typedef struct {
    char op;          // 'I' (insert), 'U' (update), 'D' (delete)
    Student before;   // State BEFORE modification
    Student after;    // State AFTER modification
} UndoEntry;

UndoEntry g_undo[1000];
int g_undo_count = 0;

/* ---------- Globals ---------- */
Student g_students[MAX_STUDENTS];
int     g_count = 0;
char    g_open_filename[260] = "";

/* ---------- Helper functions ---------- */
void rstrip(char *s){
    size_t n = strlen(s);
    while(n && (s[n-1]=='\n' || s[n-1]=='\r')) s[--n]='\0';
}
void trim(char *s){
    size_t i=0; while(s[i] && isspace((unsigned char)s[i])) i++;
    if(i) memmove(s, s+i, strlen(s+i)+1);
    size_t n=strlen(s); while(n && isspace((unsigned char)s[n-1])) s[--n]='\0';
}
int equals_ic(const char *a, const char *b){
    while(*a && *b){
        if(toupper((unsigned char)*a)!=toupper((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a=='\0' && *b=='\0';
}
int find_index_by_id(int id){
    for(int i=0;i<g_count;++i)
        if(g_students[i].id == id) return i;
    return -1;
}

/* ---------- UNDO helper ---------- */
void push_undo(char op, Student before, Student after) {
    if (g_undo_count >= 1000) return; // prevent overflow
    g_undo[g_undo_count].op = op;
    g_undo[g_undo_count].before = before;
    g_undo[g_undo_count].after = after;
    g_undo_count++;
}

/* ---------- Sorting (Bubble Sort) ---------- */
void sort_by_id(int asc){
    for(int i=0;i<g_count-1;i++){
        for(int j=0;j<g_count-1-i;j++){
            int cond = asc ? (g_students[j].id > g_students[j+1].id)
                           : (g_students[j].id < g_students[j+1].id);
            if(cond){
                Student t = g_students[j];
                g_students[j] = g_students[j+1];
                g_students[j+1] = t;
            }
        }
    }
}
void sort_by_mark(int asc){
    for(int i=0;i<g_count-1;i++){
        for(int j=0;j<g_count-1-i;j++){
            int cond = asc ? (g_students[j].mark > g_students[j+1].mark)
                           : (g_students[j].mark < g_students[j+1].mark);
            if(cond){
                Student t = g_students[j];
                g_students[j] = g_students[j+1];
                g_students[j+1] = t;
            }
        }
    }
}

void handle_sort(const char *args){
    if(!args || !*args) return;

    char buf[128];
    strncpy(buf, args, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    trim(buf);

    char *tok1 = strtok(buf, " \t");
    char *tok2 = strtok(NULL, " \t");
    char *tok3 = strtok(NULL, " \t");
    char *tok4 = strtok(NULL, " \t");

    int field = 0;   // 1=id, 2=mark
    int asc   = 1;   // default ASC

    if(tok1 && equals_ic(tok1, "SORT") &&
       tok2 && equals_ic(tok2, "BY") &&
       tok3)
    {
        if(equals_ic(tok3,"ID"))   field=1;
        if(equals_ic(tok3,"MARK")) field=2;

        if(tok4){
            if(equals_ic(tok4,"DESC")) asc=0;
            else asc=1;
        }
    }

    if(field==1) sort_by_id(asc);
    else if(field==2) sort_by_mark(asc);
}

/* ---------- load_from_file (robust parsing) ---------- */
int load_from_file(const char *filename){
    FILE *fp = fopen(filename, "r");
    if(!fp) return 0;

    g_count = 0;
    char line[LINE_MAX_LEN];
    int table_started = 0;

    while(fgets(line, sizeof(line), fp)){
        rstrip(line);

        char raw[LINE_MAX_LEN];
        strncpy(raw, line, sizeof(raw)-1);
        raw[sizeof(raw)-1] = 0;
        trim(raw);

        if(raw[0]=='\0') continue;

        /* Detect header row */
        if(!table_started){
            char up[LINE_MAX_LEN];
            strncpy(up, raw, sizeof(up)-1);
            up[sizeof(up)-1] = 0;
            for(char *u=up; *u; ++u) *u = toupper((unsigned char)*u);
            if(strstr(up,"ID") && strstr(up,"MARK")){
                table_started = 1;
            }
            continue;
        }

        if(!isdigit((unsigned char)raw[0])) continue;

        /* ----- Parse ID ----- */
        int len = strlen(raw);
        int i=0;
        while(i<len && isspace((unsigned char)raw[i])) i++;
        int id_start=i;
        while(i<len && isdigit((unsigned char)raw[i])) i++;
        int id_end=i;

        char idbuf[16];
        int idlen=id_end-id_start;
        if(idlen>=15) idlen=15;
        memcpy(idbuf, raw+id_start, idlen);
        idbuf[idlen]='\0';
        int id = atoi(idbuf);

        while(i<len && isspace((unsigned char)raw[i])) i++;
        int mid_start=i;

        /* ----- Parse mark from right ----- */
        int j=len-1;
        while(j>=0 && isspace((unsigned char)raw[j])) j--;
        int mark_end=j+1;

        int mark_start=j;
        while(mark_start>=0 &&
              (isdigit((unsigned char)raw[mark_start]) || raw[mark_start]=='.'))
            mark_start--;
        mark_start++;

        char markbuf[16];
        int marklen = mark_end - mark_start;
        if(marklen>=15) marklen=15;
        memcpy(markbuf, raw+mark_start, marklen);
        markbuf[marklen]='\0';
        float mark = atof(markbuf);

        /* ----- Middle (Name + Programme) ----- */
        int mid_end = mark_start;
        char middle[LINE_MAX_LEN];
        int midlen = mid_end-mid_start;
        if(midlen>=LINE_MAX_LEN) midlen=LINE_MAX_LEN-1;
        memcpy(middle, raw+mid_start, midlen);
        middle[midlen]='\0';
        trim(middle);

        char name[NAME_MAX_LEN]="";
        char prog[PROG_MAX_LEN]="";

        /* Look for 2+ spaces as separator */
        int sep=-1;
        for(int k=0; middle[k] && middle[k+1]; ++k){
            if(middle[k]==' ' && middle[k+1]==' '){
                sep=k;
                break;
            }
        }

        if(sep>=0){
            int nlen=sep;
            while(nlen>0 && isspace((unsigned char)middle[nlen-1])) nlen--;
            memcpy(name, middle, nlen);
            name[nlen]='\0';

            int pstart=sep;
            while(middle[pstart] && isspace((unsigned char)middle[pstart])) pstart++;
            strncpy(prog, middle+pstart, PROG_MAX_LEN-1);
            prog[PROG_MAX_LEN-1] = '\0';
        } else {
            /* Fallback: first two words = name, rest = programme */
            char tmp[LINE_MAX_LEN];
            strncpy(tmp, middle, sizeof(tmp)-1);
            tmp[sizeof(tmp)-1] = 0;

            char *p=tmp;
            while(*p && isspace((unsigned char)*p)) p++;
            char *w1=p;
            while(*p && !isspace((unsigned char)*p)) p++;
            if(*p) *p++='\0';

            while(*p && isspace((unsigned char)*p)) p++;
            char *w2=p;
            while(*p && !isspace((unsigned char)*p)) p++;
            if(*p) *p++='\0';

            snprintf(name, sizeof(name), "%s %s", w1, w2);

            while(*p && isspace((unsigned char)*p)) p++;
            strncpy(prog, p, PROG_MAX_LEN-1);
            prog[PROG_MAX_LEN-1]='\0';
        }

        trim(name);
        trim(prog);

        if(g_count < MAX_STUDENTS){
            Student s;
            s.id=id;
            strncpy(s.name,name,NAME_MAX_LEN-1);
            s.name[NAME_MAX_LEN-1]=0;
            strncpy(s.programme,prog,PROG_MAX_LEN-1);
            s.programme[PROG_MAX_LEN-1]=0;
            s.mark=mark;
            g_students[g_count++] = s;
        }
    }

    fclose(fp);
    return 1;
}

/* ---------- SAVE ---------- */
int save_to_file(const char *filename){
    FILE *fp = fopen(filename,"w");
    if(!fp) return 0;

    fprintf(fp,"Database Name: StudentRecords\nAuthors: Team\n\n");
    fprintf(fp,"Table Name: StudentRecords\n");
    fprintf(fp,"ID\tName\tProgramme\tMark\n");

    for(int i=0;i<g_count;i++){
        fprintf(fp,"%d\t%s\t%s\t%.1f\n",
            g_students[i].id,
            g_students[i].name,
            g_students[i].programme,
            g_students[i].mark
        );
    }

    fclose(fp);
    return 1;
}

/* ===================== COMMANDS ===================== */
void show_help(void){
    printf("\nAvailable Commands:\n");
    printf("  OPEN <filename>   -> open the database file and read in all records\n");
    printf("  SHOW ALL          -> display all current records in memory\n");
    printf("  SORT BY ID[ASC|DESC] -> sort all current records in memory by ID\n");
    printf("  SORT BY MARK[ASC|DESC]-> sort all current records in memory by Mark\n");
    printf("  INSERT            -> insert a new record (prompts every column)\n");
    printf("  QUERY ID=<n>      -> search for a record with a given student ID\n");
    printf("  UPDATE ID=<n>     -> update the data (prompts every column; Enter keeps)\n");
    printf("  DELETE ID=<n>     -> delete the record (double confirm)\n");
    printf("  SAVE              -> save all current records into the database file\n");
    printf("  UNDO              -> undo the last INSERT, UPDATE, or DELETE\n");
    printf("  HELP / EXIT       -> help or quit\n\n");
}

/* ---------- OPEN ---------- */
void cmd_open(const char *args){
    char fname[260]="";

    while(*args && isspace((unsigned char)*args)) args++;

    size_t i=0;
    while(*args && !isspace((unsigned char)*args) && i<sizeof(fname)-1)
        fname[i++]=*args++;
    fname[i]='\0';

    if(fname[0]=='\0'){
        printf("CMS: Please provide a filename.\n");
        return;
    }

    if(!load_from_file(fname)){
        strncpy(g_open_filename,fname,sizeof(g_open_filename)-1);
        g_open_filename[sizeof(g_open_filename)-1]='\0';
        printf("CMS: File not found — will create new on SAVE.\n");
        return;
    }

    strncpy(g_open_filename,fname,sizeof(g_open_filename)-1);
    g_open_filename[sizeof(g_open_filename)-1]='\0';
    printf("CMS: The database file \"%s\" is successfully opened. (%d records loaded)\n",
           fname, g_count);
}

/* ---------- SHOW ALL ---------- */
void cmd_show_all(const char *args){
    if(g_count==0){
        printf("CMS: No records loaded.\n");
        return;
    }

    handle_sort(args);

    printf("CMS: Here are all the records.\n");
    printf("%-10s %-20s %-25s %-6s\n", "ID","Name","Programme","Mark");
    
    for(int i=0;i<g_count;i++){
        printf("%-10d %-20s %-25s %-6.1f\n",
               g_students[i].id,
               g_students[i].name,
               g_students[i].programme,
               g_students[i].mark);
    }
}

/* ---------- INSERT ---------- */
void prompt_string(const char *label,char*out,size_t outsz){
    while(1){
        printf("%s", label);
        if(!fgets(out,outsz,stdin)) { out[0]=0; return; }
        rstrip(out); trim(out);
        if(out[0]) return;
        printf("Please enter something.\n");
    }
}
int prompt_int(const char *label){
    char buf[64];
    while(1){
        printf("%s",label);
        if(!fgets(buf,sizeof(buf),stdin)) return 0;
        rstrip(buf); trim(buf);
        char *e; long v=strtol(buf,&e,10);
        if(*e=='\0') return (int)v;
        printf("Invalid integer.\n");
    }
}
float prompt_float(const char *label){
    char buf[64];
    while(1){
        printf("%s",label);
        if(!fgets(buf,sizeof(buf),stdin)) return 0;
        rstrip(buf); trim(buf);
        char *e; float v=strtof(buf,&e);
        if(*e=='\0') return v;
        printf("Invalid number.\n");
    }
}

void cmd_insert(const char *args){
    if(g_count>=MAX_STUDENTS){
        printf("CMS: Storage full.\n");
        return;
    }

    int id = prompt_int("Enter student ID: ");

    if(find_index_by_id(id)>=0){
        printf("CMS: Same ID exists. Insert cancelled.\n");
        return;
    }

    char name[NAME_MAX_LEN];
    char prog[PROG_MAX_LEN];
    float mark;

    prompt_string("Enter Name: ", name, sizeof(name));
    prompt_string("Enter Programme: ", prog, sizeof(prog));
    mark = prompt_float("Enter Mark: ");

    Student s;
    s.id=id;
    strncpy(s.name,name,NAME_MAX_LEN-1);
    s.name[NAME_MAX_LEN-1]=0;
    strncpy(s.programme,prog,PROG_MAX_LEN-1);
    s.programme[PROG_MAX_LEN-1]=0;
    s.mark=mark;

    g_students[g_count++] = s;
    push_undo('I', s, s);     // UNDO support for INSERT
    printf("CMS: Record inserted.\n");
}

/* ---------- QUERY ---------- */
void cmd_query(const char *args){
    int id = prompt_int("Enter student ID to search: ");
    int idx = find_index_by_id(id);

    if(idx<0){
        printf("CMS: No record found.\n");
        return;
    }

    Student *s=&g_students[idx];
    printf("Record found:\n");
    printf("ID\tName\tProgramme\tMark\n");
    printf("%d\t%s\t%s\t%.1f\n",s->id,s->name,s->programme,s->mark);
}

/* ---------- UPDATE ---------- */
float prompt_edit_float(const char *label,float cur){
    char buf[64];
    printf("%s (current %.1f) -> ",label,cur);
    if(!fgets(buf,sizeof(buf),stdin)) return cur;
    rstrip(buf); trim(buf);
    if(buf[0]=='\0') return cur;
    char *e; float v=strtof(buf,&e);
    if(*e=='\0') return v;
    printf("Invalid. Keeping old.\n");
    return cur;
}
void prompt_edit_string(const char *label,const char*cur,char*out,size_t outsz){
    printf("%s (current: %s) -> ",label,cur);
    if(!fgets(out,outsz,stdin)){ out[0]=0; return; }
    rstrip(out); trim(out);
}

void cmd_update(const char *args){
    int id = prompt_int("Enter student ID to update: ");
    int idx = find_index_by_id(id);

    if(idx<0){
        printf("CMS: No record found.\n");
        return;
    }

    Student *s=&g_students[idx];

    Student old = *s;   // store BEFORE state

    char newName[NAME_MAX_LEN]="";
    char newProg[PROG_MAX_LEN]="";

    prompt_edit_string("Enter new Name", s->name, newName, sizeof(newName));
    if(newName[0]) strncpy(s->name,newName,sizeof(s->name)-1);

    prompt_edit_string("Enter new Programme", s->programme, newProg, sizeof(newProg));
    if(newProg[0]) strncpy(s->programme,newProg,sizeof(s->programme)-1);

    s->mark = prompt_edit_float("Enter new Mark", s->mark);

    push_undo('U', old, *s);   // store BEFORE & AFTER for undo
    printf("CMS: Record updated.\n");
}

/* ---------- DELETE ---------- */
void cmd_delete(const char *args){
    int id = prompt_int("Enter student ID to delete: ");
    int idx = find_index_by_id(id);

    if(idx<0){
        printf("CMS: No record found.\n");
        return;
    }

    printf("Are you sure? (Y/N): ");
    char b[8];
    fgets(b,sizeof(b),stdin);
    if(toupper(b[0])!='Y'){
        printf("Delete cancelled.\n");
        return;
    }

    printf("Confirm again (Y/N): ");
    fgets(b,sizeof(b),stdin);
    if(toupper(b[0])!='Y'){
        printf("Delete cancelled.\n");
        return;
    }

    Student removed = g_students[idx];
    push_undo('D', removed, removed);   // UNDO for DELETE

    for(int i=idx;i<g_count-1;i++)
        g_students[i]=g_students[i+1];
    g_count--;

    printf("CMS: Record deleted.\n");
}

/* ---------- SAVE ---------- */
void cmd_save(void){
    if(g_open_filename[0]=='\0'){
        printf("CMS: No file opened.\n");
        return;
    }
    if(save_to_file(g_open_filename))
        printf("CMS: Saved.\n");
    else
        printf("CMS: Save failed.\n");
}

/* ---------- UNDO ---------- */
void cmd_undo(void) {
    if (g_undo_count == 0) {
        printf("CMS: Nothing to undo.\n");
        return;
    }

    UndoEntry last = g_undo[g_undo_count - 1];
    g_undo_count--;

    if (last.op == 'I') {
        // Undo INSERT → remove inserted student
        int idx = find_index_by_id(last.after.id);
        if (idx >= 0) {
            for (int i = idx; i < g_count - 1; i++)
                g_students[i] = g_students[i + 1];
            g_count--;
        }
        printf("CMS: Undo successful (INSERT undone).\n");
    }

    else if (last.op == 'D') {
        // Undo DELETE → restore deleted student
        if (g_count < MAX_STUDENTS) {
            g_students[g_count++] = last.before;
            printf("CMS: Undo successful (DELETE undone).\n");
        } else {
            printf("CMS: Undo failed (storage full).\n");
        }
    }

    else if (last.op == 'U') {
        // Undo UPDATE → revert back to old state
        int idx = find_index_by_id(last.after.id);
        if (idx >= 0) {
            g_students[idx] = last.before;
            printf("CMS: Undo successful (UPDATE undone).\n");
        } else {
            printf("CMS: Undo failed (record not found).\n");
        }
    }

    else {
        printf("CMS: Undo failed (unknown operation).\n");
    }
}

/* ---------- PRINT DECLARATION ---------- */
void print_declaration(void){
    FILE *fp=fopen("declaration.txt","r");
    if(!fp){
        printf("Error: declaration.txt not found.\n\n");
        return;
    }
    char line[256];
    while(fgets(line,sizeof(line),fp))
        printf("%s",line);
    fclose(fp);
    printf("\n");
}

/* ---------- MAIN ---------- */
int main(void){
    print_declaration();
    show_help();

    char line[LINE_MAX_LEN];

    while(1){
        printf("> ");
        if(!fgets(line,sizeof(line),stdin)) break;
        rstrip(line);
        if(line[0]=='\0') continue;

        char cmd[64];
        int i=0;
        const char *p=line;

        while(*p && isspace((unsigned char)*p)) p++;
        while(*p && !isspace((unsigned char)*p) && i<63)
            cmd[i++]=*p++;
        cmd[i]=0;
        while(*p && isspace((unsigned char)*p)) p++;

        if(equals_ic(cmd,"EXIT")) break;
        else if(equals_ic(cmd,"HELP")) show_help();
        else if(equals_ic(cmd,"OPEN")) cmd_open(p);
        else if(equals_ic(cmd,"SHOW")) {
            char first[16];
            int fi=0;
            const char *q=p;

            while(*q && !isspace((unsigned char)*q) && fi<15)
                first[fi++]=*q++;
            first[fi]=0;
            while(*q && isspace((unsigned char)*q)) q++;

            if(equals_ic(first,"ALL")) cmd_show_all(q);
            else printf("CMS: Use SHOW ALL.\n");
        }
        else if(equals_ic(cmd,"INSERT")) cmd_insert(p);
        else if(equals_ic(cmd,"QUERY"))  cmd_query(p);
        else if(equals_ic(cmd,"UPDATE")) cmd_update(p);
        else if(equals_ic(cmd,"DELETE")) cmd_delete(p);
        else if(equals_ic(cmd,"SAVE"))   cmd_save();
        else if(equals_ic(cmd,"UNDO"))   cmd_undo();
        else printf("CMS: Unknown command.\n");
    }

    return 0;
}

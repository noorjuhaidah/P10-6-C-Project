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
//  LOGIN — This feature allows users to securely authenticate as either an admin or a student 
//          by providing a valid username and password. Admin users can access all CMS features 
//          (INSERT, DELETE, UPDATE, SAVE), while students have restricted access (can only view records).
//          The login process is protected by a limit of 5 attempts, after which the system locks further login attempts.
//          Demonstrates use of input validation, role-based access control, security measures in user authentication,
//          and a mechanism to prevent brute force attacks through the attempt limitation.


#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>   // for roundf()
#define ADMIN_USERNAME "admin"
#define ADMIN_PASSWORD "admin"  // admin password
#define STUDENT_USERNAME "student"
#define STUDENT_PASSWORD "student"  // student password
#define DEFAULT_STUDENT_DB "P10_6-cms.txt"

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
int     g_is_admin = 0; // 0 - student, 1 - admin

/* ---------- Helper functions ---------- */
void rstrip(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

void trim(char *s) {
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, strlen(s + i) + 1);
    size_t n = strlen(s); while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

int equals_ic(const char *a, const char *b) {
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

int find_index_by_id(int id) {
    for (int i = 0; i < g_count; ++i)
        if (g_students[i].id == id) return i;
    return -1;
}

/* ---------- User Login Function ---------- */
int login() {
    char username[64], password[64];
    int attempts = 5;  // Max attempts: 5 total attempts (for password only)

    while (attempts > 0) {
        printf("Enter username: ");
        fgets(username, sizeof(username), stdin);
        rstrip(username);

        // Check for invalid username
        if (!equals_ic(username, ADMIN_USERNAME) && !equals_ic(username, STUDENT_USERNAME)) {
            printf("Invalid username. Please enter a valid username.\n");
            continue; // Skip password check and allow retry without decrementing attempts
        }

        // Admin login
        if (equals_ic(username, ADMIN_USERNAME)) {
            printf("Enter password: ");
            fgets(password, sizeof(password), stdin);
            rstrip(password);

            if (equals_ic(password, ADMIN_PASSWORD)) {
                g_is_admin = 1;  // Admin login
                return 1;
            } else {
                printf("Invalid admin password. Try again.\n");
            }
        }

        // Student login
        else if (equals_ic(username, STUDENT_USERNAME)) {
            printf("Enter password: ");
            fgets(password, sizeof(password), stdin);
            rstrip(password);

            if (equals_ic(password, STUDENT_PASSWORD)) {
                g_is_admin = 0;  // Student login
                return 1;
            } else {
                printf("Invalid student password. Try again.\n");
            }
        }      

        // If password is invalid, decrease the attempts
        printf("You have %d attempt(s) left.\n", attempts - 1);
        attempts--;  // Decrease the remaining attempts

        // Exit after 0 attempts left with a more professional message
        if (attempts == 0) {
            printf("Maximum login attempts reached. Access to the system has been temporarily locked. Please try again later or contact support.\n");
            return 0;  // Exit the program after exceeding max attempts
        }
    }

    return 0;  // If all attempts are exhausted, return failure
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
    printf("\n--------------------------------------------------------------------------------");
    printf("\nAvailable Commands:\n");
    printf("\n                 ---File Operations---                           \n");
    printf("  OPEN <filename>              -> open the database file and read in all records\n");
    printf("\n                 ---Display Operations---                    \n");
    printf("  SHOW ALL                     -> display all current records in memory\n");
    printf("  SHOW ALL SORT BY ID ASC      -> sort by student ID (ascending)\n");
    printf("  SHOW ALL SORT BY ID DESC     -> sort by student ID (descending)\n");
    printf("  SHOW ALL SORT BY MARK ASC    -> sort by mark (ascending)\n");
    printf("  SHOW ALL SORT BY MARK DESC   -> sort by mark (descending)\n");
    printf("  SHOW SUMMARY                 -> show total, average mark, highest & lowest\n");
    printf("\n                 ---Record Operations---                    \n");
    printf("  INSERT                       -> insert a new record (prompts every column)\n");
    printf("  QUERY ID=<n>                 -> search for a record with a given student ID\n");
    printf("  UPDATE ID=<n>                -> update the data (prompts every column; Enter keeps)\n");
    printf("  DELETE ID=<n>                -> delete the record (double confirm)\n");
    printf("  SAVE                         -> save all current records into the database file\n");
    printf("  UNDO                         -> undo the last INSERT, UPDATE, or DELETE\n");   
    printf("\n                      ---General---                           \n");
    printf("  HELP                         -> show this help menu\n");
    printf("  EXIT                         -> quit the program\n");
    printf("--------------------------------------------------------------------------------\n");
}

void show_help_student(void){
    printf("\n--------------------------------------------------------------------------------");
    printf("\nAvailable Commands (Student Access Only):\n");   
    printf("\n                 ---Display Operations---                    \n");
    printf("  SHOW ALL                     -> display all current records in memory\n");
    printf("  SHOW ALL SORT BY ID ASC      -> sort by student ID (ascending)\n");
    printf("  SHOW ALL SORT BY ID DESC     -> sort by student ID (descending)\n");
    printf("  SHOW ALL SORT BY MARK ASC    -> sort by mark (ascending)\n");
    printf("  SHOW ALL SORT BY MARK DESC   -> sort by mark (descending)\n");
    printf("  SHOW SUMMARY                 -> show total, average, highest & lowest marks\n");
    printf("\n                     ---Search---                           \n");
    printf("  QUERY ID=<n>                 -> search for a specific student record\n");   
    printf("\n                     ---General---                           \n");
    printf("  HELP                         -> show this help menu\n");
    printf("  EXIT                         -> quit the program\n");
    printf("--------------------------------------------------------------------------------\n");
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
// ================== NEW VALIDATION FUNCTIONS ==================

// Check string contains only letters and spaces (no digits/symbols)
int is_alpha_space(const char *s){
    if (s[0] == '\0') return 0; // empty not allowed

    for (int i = 0; s[i] != '\0'; i++){
        unsigned char c = (unsigned char)s[i];
        if (!isalpha(c) && !isspace(c)) {
            return 0;
        }
    }
    return 1;
}

// NEW: validate student ID (must start with 2 + 7 digits)
int prompt_student_id(void){
    char buf[64];

    while(1){
        printf("Enter student ID: ");
        if(!fgets(buf,sizeof(buf),stdin)) return 0;
        rstrip(buf);
        trim(buf);

        int len = strlen(buf);
        int ok = 1;

        if(len != 7 || buf[0] != '2'){
            ok = 0;
        } else {
            for(int i=0;i<len;i++){
                if(!isdigit((unsigned char)buf[i])){
                    ok = 0;
                    break;
                }
            }
        }

        if(!ok){
            printf("Error: Student ID must start with 2 and have exactly 7 digits.\n");
            continue;
        }

        return (int)strtol(buf,NULL,10);
    }
}

// NEW: validate and round mark to 1 decimal place
float prompt_mark(const char *label){
    char buf[64];

    while(1){
        printf("%s", label);
        if(!fgets(buf,sizeof(buf),stdin)) return 0;
        rstrip(buf);
        trim(buf);

        char *e;
        float v = strtof(buf,&e);

        if(*e=='\0'){
            v = roundf(v * 10.0f) / 10.0f; // round to 1dp
            return v;
        }

        printf("Invalid number.\n");
    }
}

// NEW: validated name input (letters + spaces)
void prompt_name(char *out, size_t outsz){
    while(1){
        prompt_string("Enter Name: ", out, outsz);

        if(is_alpha_space(out)){
            return;
        }
        printf("Error: Name must contain only letters and spaces.\n");
    }
}

// NEW: validated programme input (letters + spaces)
void prompt_programme(char *out, size_t outsz){
    while(1){
        prompt_string("Enter Programme: ", out, outsz);

        if(is_alpha_space(out)){
            return;
        }
        printf("Error: Programme must contain only letters and spaces.\n");
    }
}

/* ---------- INSERT ---------- */
void cmd_insert(const char *args){
    if(g_count >= MAX_STUDENTS){
        printf("CMS: Storage full.\n");
        return;
    }

    int id;

    // Validate ID + check duplicate
    while(1){
        id = prompt_student_id();
        if(find_index_by_id(id) >= 0){
            printf("Error: This ID exists.\n");
        } else {
            break;
        }
    }

    char name[NAME_MAX_LEN];
    char prog[PROG_MAX_LEN];
    float mark;

    // NEW VALIDATED INPUTS
    prompt_name(name, sizeof(name));
    prompt_programme(prog, sizeof(prog));
    mark = prompt_mark("Enter Mark: ");

    Student s;
    s.id = id;
    strncpy(s.name,name,NAME_MAX_LEN-1);
    s.name[NAME_MAX_LEN-1]=0;
    strncpy(s.programme,prog,PROG_MAX_LEN-1);
    s.programme[PROG_MAX_LEN-1]=0;
    s.mark = mark;

    g_students[g_count++] = s;
    push_undo('I', s, s);

    printf("CMS: Record inserted.\n");
    printf("Remember to type SAVE to save your changes.\n");
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
/* ---------- YES/NO VALIDATOR ---------- */
int prompt_yes_no(const char *msg) {
    char buf[16];
    while (1) {
        printf("%s (Y/N): ", msg);
        if (!fgets(buf, sizeof(buf), stdin)) continue;
        rstrip(buf);
        trim(buf);

        if (toupper(buf[0]) == 'Y') return 1;
        if (toupper(buf[0]) == 'N') return 0;

        printf("Invalid choice. Please enter Y or N.\n");
    }
}

/* ---------- UPDATE (fully rewritten) ---------- */
void cmd_update(const char *args){
    int id = prompt_int("Enter student ID to update: ");
    int idx = find_index_by_id(id);

    if (idx < 0) {
        printf("CMS: No record found.\n");
        return;
    }

    Student *s = &g_students[idx];

    /* ----- Show record BEFORE update ----- */
    printf("\nRecord found:\n");
    printf("ID      : %d\n", s->id);
    printf("Name    : %s\n", s->name);
    printf("Programme: %s\n", s->programme);
    printf("Mark    : %.1f\n\n", s->mark);

    Student old = *s;     // backup for undo
    Student updated = *s; // temp copy for editing

    int changed = 0;

    /* ===========================
       UPDATE STUDENT ID
       =========================== */
    if (prompt_yes_no("Do you want to update the student ID?")) {

        int newID;

        while (1) {
            newID = prompt_student_id();  // validated student ID format

            // ensure this ID isn't used by other students
            int exist = find_index_by_id(newID);
            if (exist >= 0 && exist != idx) {
                printf("Error: This ID already exists.\n");
                continue;
            }
            break;
        }

        if (newID != updated.id) {
            updated.id = newID;
            changed = 1;
        } else {
            printf("No change detected for ID.\n");
        }
    }

    /* ===========================
       UPDATE NAME
       =========================== */
    if (prompt_yes_no("Do you want to update the Name?")) {

        char temp[NAME_MAX_LEN];

        while (1) {
            printf("Enter new Name (current: %s): ", updated.name);
            if (!fgets(temp, sizeof(temp), stdin)) continue;
            rstrip(temp);
            trim(temp);

            if (temp[0] == '\0') {
                printf("Name cannot be empty.\n");
                continue;
            }

            if (!is_alpha_space(temp)) {
                printf("Error: Name must only contain letters and spaces.\n");
                continue;
            }

            break;
        }

        if (strcmp(temp, updated.name) != 0) {
            strncpy(updated.name, temp, NAME_MAX_LEN - 1);
            updated.name[NAME_MAX_LEN - 1] = '\0';
            changed = 1;
        } else {
            printf("No change detected for Name.\n");
        }
    }

    /* ===========================
       UPDATE PROGRAMME
       =========================== */
    if (prompt_yes_no("Do you want to update the Programme?")) {

        char temp[PROG_MAX_LEN];

        while (1) {
            printf("Enter new Programme (current: %s): ", updated.programme);
            if (!fgets(temp, sizeof(temp), stdin)) continue;
            rstrip(temp);
            trim(temp);

            if (temp[0] == '\0') {
                printf("Programme cannot be empty.\n");
                continue;
            }

            if (!is_alpha_space(temp)) {
                printf("Error: Programme must only contain letters and spaces.\n");
                continue;
            }

            break;
        }

        if (strcmp(temp, updated.programme) != 0) {
            strncpy(updated.programme, temp, PROG_MAX_LEN - 1);
            updated.programme[PROG_MAX_LEN - 1] = '\0';
            changed = 1;
        } else {
            printf("No change detected for Programme.\n");
        }
    }

    /* ===========================
       UPDATE MARK
       =========================== */
    if (prompt_yes_no("Do you want to update the Mark?")) {

        while (1) {
            char buf[64];
            printf("Enter new Mark (current: %.1f): ", updated.mark);

            if (!fgets(buf, sizeof(buf), stdin)) continue;
            rstrip(buf);
            trim(buf);

            if (buf[0] == '\0') {
                printf("Mark cannot be empty.\n");
                continue;
            }

            char *end;
            float v = strtof(buf, &end);

            if (*end != '\0' || v < 0 || v > 100) {
                printf("Invalid mark. Must be 0–100.\n");
                continue;
            }

            v = roundf(v * 10.0f) / 10.0f; // 1dp rounding

            if (v != updated.mark) {
                updated.mark = v;
                changed = 1;
            } else {
                printf("No change detected for Mark.\n");
            }
            break;
        }
    }

    /* ===========================
       NO CHANGES?
       =========================== */
    if (!changed) {
        printf("\nCMS: No changes made. Update cancelled.\n");
        return;
    }

    /* ===========================
       APPLY UPDATE + UNDO
       =========================== */
    *s = updated; // commit changes

    push_undo('U', old, *s);

    printf("\nCMS: Record updated successfully.\n");
    printf("Remember to type SAVE to save your changes.\n");

    /* ===========================
       SHOW UPDATED RECORD
       =========================== */
    printf("Updated Record:\n");
    printf("ID       : %d\n", s->id);
    printf("Name     : %s\n", s->name);
    printf("Programme: %s\n", s->programme);
    printf("Mark     : %.1f\n", s->mark);
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
    printf("Remember to type SAVE to save your changes.\n");
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
        printf("CMS: No actions to undo.\n");
        return;
    }

    UndoEntry last = g_undo[g_undo_count - 1];
    g_undo_count--;

    // Display the most recent amendment in a clear, professional format
    printf("\n--------------------------------------------------\n");
    printf("   MOST RECENT AMENDMENT DETAILS\n");
    printf("--------------------------------------------------\n");

    if (last.op == 'I') {
        // INSERT operation
        printf("Operation:  Insert\n");
        printf("Student ID: %d\n", last.after.id);
        printf("Name:       %s\n", last.after.name);
        printf("Programme:  %s\n", last.after.programme);
        printf("Mark:       %.1f\n", last.after.mark);
    } else if (last.op == 'U') {
        // UPDATE operation
        printf("Operation:  Update\n");
        printf("Student ID: %d\n", last.before.id);
        printf("Name:       %s -> %s\n", last.before.name, last.after.name);
        printf("Programme:  %s -> %s\n", last.before.programme, last.after.programme);
        printf("Mark:       %.1f -> %.1f\n", last.before.mark, last.after.mark);
    } else if (last.op == 'D') {
        // DELETE operation
        printf("Operation:  Delete\n");
        printf("Student ID: %d\n", last.before.id);
        printf("Name:       %s\n", last.before.name);
        printf("Programme:  %s\n", last.before.programme);
        printf("Mark:       %.1f\n", last.before.mark);
    }

    printf("--------------------------------------------------\n");

    // Ask for confirmation
    if (prompt_yes_no("Do you want to undo this action?")) {
        if (last.op == 'I') {
            // Undo INSERT → remove inserted student
            int idx = find_index_by_id(last.after.id);
            if (idx >= 0) {
                for (int i = idx; i < g_count - 1; i++)
                    g_students[i] = g_students[i + 1];
                g_count--;
            }
            printf("CMS: Undo successful (INSERT undone).\n");
        } else if (last.op == 'D') {
            // Undo DELETE → restore deleted student
            if (g_count < MAX_STUDENTS) {
                g_students[g_count++] = last.before;
                printf("CMS: Undo successful (DELETE undone).\n");
            } else {
                printf("CMS: Undo failed (storage full).\n");
            }
        } else if (last.op == 'U') {
            // Undo UPDATE → revert back to old state
            int idx = find_index_by_id(last.after.id);
            if (idx >= 0) {
                g_students[idx] = last.before;
                printf("CMS: Undo successful (UPDATE undone).\n");
            } else {
                printf("CMS: Undo failed (record not found).\n");
            }
        }
        printf("Remember to type SAVE to save your changes.\n");
    } else {
        printf("Undo cancelled.\n");
    }
}

void cmd_show_summary(void) {
    if (g_count == 0) {
        printf("CMS: No records loaded.\n");
        return;
    }

    int count = g_count;

    float sum = 0.0f;
    int idx_max = 0;   // index of the highest mark
    int idx_min = 0;   // index of the lowest mark

    // loop through all records to find sum, min, max
    for (int i = 0; i < count; i++) {
        float mark = g_students[i].mark;
        sum += mark;

        if (mark > g_students[idx_max].mark) {
            idx_max = i;
        }
        if (mark < g_students[idx_min].mark) {
            idx_min = i;
        }
    }

    float average = sum / count;

    Student *s_max = &g_students[idx_max];
    Student *s_min = &g_students[idx_min];

    printf("CMS SUMMARY\n");
    printf("-----------\n");
    printf("Total number of students : %d\n", count);
    printf("Average mark             : %.2f\n", average);
    printf("Highest mark             : %.1f (Student ID: %d, Name: %s)\n",
           s_max->mark, s_max->id, s_max->name);
    printf("Lowest mark              : %.1f (Student ID: %d, Name: %s)\n",
           s_min->mark, s_min->id, s_min->name);
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
int main(void) {
    // Call the login function
    if (!login()) return 0;  // If login fails, exit the program

    print_declaration();

    // Auto-open for student
    if (!g_is_admin) {
        if (!load_from_file(DEFAULT_STUDENT_DB)) {
            printf("CMS: Auto-load failed. Creating new DB on SAVE.\n");
        } else {
            printf("CMS: P10_6-CMS loaded successfully (%d records).\n", g_count);
        }
        strncpy(g_open_filename, DEFAULT_STUDENT_DB, sizeof(g_open_filename)-1);
        g_open_filename[sizeof(g_open_filename)-1] = '\0';
    }

    // Show help based on login type (admin or student)
    if (g_is_admin) {
        show_help();           // admin gets full help
    } else {
        show_help_student();   // student gets restricted help
    }

    char line[LINE_MAX_LEN];

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        rstrip(line);
        if (line[0] == '\0') continue;

        char cmd[64];
        int i = 0;
        const char *p = line;

        while (*p && isspace((unsigned char)*p)) p++;
        while (*p && !isspace((unsigned char)*p) && i < 63)
            cmd[i++] = *p++;
        cmd[i] = 0;
        while (*p && isspace((unsigned char)*p)) p++;

        // Command processing
        if (equals_ic(cmd, "EXIT")) break;
        else if (equals_ic(cmd, "HELP")) {
            if (g_is_admin)
                show_help();           // admin gets full help
            else
                show_help_student();   // student gets restricted help
        }
        else if (equals_ic(cmd, "OPEN")) {
            if (g_is_admin) {
                cmd_open(p);
            } else {
                printf("Students cannot open database files (auto-loaded at login).\n");
            }
        }
        else if (equals_ic(cmd, "SHOW")) {
            if (*p == '\0') {
                printf("CMS: Use SHOW ALL or SHOW SUMMARY.\n");
            } else {
                // Handle SHOW commands
                char first[16];
                int fi = 0;
                const char *q = p;

                while (*q && !isspace((unsigned char)*q) && fi < (int)sizeof(first) - 1) {
                    first[fi++] = *q++;
                }
                first[fi] = '\0';
                while (*q && isspace((unsigned char)*q)) q++;

                if (equals_ic(first, "ALL")) {
                    cmd_show_all(q);
                } else if (equals_ic(first, "SUMMARY")) {
                    cmd_show_summary();
                } else {
                    printf("CMS: Use SHOW ALL or SHOW SUMMARY.\n");
                }
            }
        }
        else if (equals_ic(cmd, "INSERT")) {
            if (g_is_admin) {
                cmd_insert(p); // Only admins can insert records
            } else {
                printf("You do not have permission to insert records.\n"); // Students cannot insert
            }
        }
        else if (equals_ic(cmd, "DELETE")) {
            if (g_is_admin) {
                cmd_delete(p); // Only admins can delete records
            } else {
                printf("You do not have permission to delete records.\n"); // Students cannot delete
            }
        }
        else if (equals_ic(cmd, "UNDO")) {
            if (g_is_admin) {
                cmd_undo(); // Only admins can undo actions
            } else {
                printf("You do not have permission to undo actions.\n"); // Students cannot undo
            }
        }
        else if (equals_ic(cmd, "SAVE")) {
            if (g_is_admin) {
                cmd_save(); // Only admins can save changes
            } else {
                printf("You do not have permission to save changes.\n"); // Students cannot save
            }
        }
        else if (equals_ic(cmd, "QUERY")) {
            cmd_query(p);
        }
        else if (equals_ic(cmd, "UPDATE")) {
            if (g_is_admin) {
                cmd_update(p);  // Only admins can update records
            } else {
                printf("You do not have permission to update records.\n");  // Students cannot update
            }
        }
        else {
            printf("CMS: Unknown command or insufficient permissions.\n");
        }
    }

    return 0;
} 
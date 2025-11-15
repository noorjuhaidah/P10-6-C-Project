// Main.c — SUPER BEGINNER CMS (no 'static', interactive, file-friendly)
// Build:  gcc Main.c -o CMS
// Run:    ./CMS
//
// Features (match assignment table):
//  OPEN <filename>  -> open the database file and read in all records
//  SHOW ALL         -> display all current records in memory
//  INSERT           -> if same ID exists: error+cancel; else PROMPT every column
//  QUERY ID=<n>     -> show record if found; else "no record found"
//  UPDATE ID=<n>    -> if not found: warn; else PROMPT every column (Enter = keep)
//  DELETE ID=<n>    -> if not found: warn; else double-confirm then delete
//  SAVE             -> save all current records back to the same file
//  HELP / EXIT

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

/* ---------- Globals ---------- */
Student g_students[MAX_STUDENTS];
int     g_count = 0;
char    g_open_filename[260] = "";

/* ---------- Small helpers ---------- */
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
    while(*a && *b){ if(toupper((unsigned char)*a)!=toupper((unsigned char)*b)) return 0; a++; b++; }
    return *a=='\0' && *b=='\0';
}
/* Turn any TAB or run of >=2 spaces into one '\t'. Keeps single spaces. */
void normalize_delims(char *s){
    char out[LINE_MAX_LEN]; size_t oi=0; int spaces=0;
    for(size_t i=0; s[i] && oi+1<sizeof(out); ++i){
        unsigned char c=(unsigned char)s[i];
        if(c=='\t'){ out[oi++]='\t'; spaces=0; continue; }
        if(c==' '){ spaces++; continue; }
        if(spaces>=2) out[oi++]='\t'; else if(spaces==1) out[oi++]=' ';
        spaces=0; out[oi++]=(char)c;
    }
    if(spaces>=2) out[oi++]='\t'; else if(spaces==1) out[oi++]=' ';
    out[oi]='\0'; strncpy(s,out,LINE_MAX_LEN-1); s[LINE_MAX_LEN-1]=0;
}
int find_index_by_id(int id){
    for(int i=0;i<g_count;++i) if(g_students[i].id==id) return i;
    return -1;
}

/* ---------- Friendly prompts (used by INSERT/UPDATE) ---------- */
void prompt_string(const char *label, char *out, size_t outsz){
    while(1){
        printf("%s", label);
        if(!fgets(out,(int)outsz,stdin)){ out[0]='\0'; return; }
        rstrip(out); trim(out);
        if(out[0]) return;
        printf("Please enter something (cannot be empty).\n");
    }
}
int prompt_int(const char *label){
    char buf[128];
    while(1){
        printf("%s", label);
        if(!fgets(buf,sizeof(buf),stdin)) return 0;
        rstrip(buf); trim(buf);
        char *end=NULL; long v=strtol(buf,&end,10);
        if(end && (*end=='\0')) return (int)v;
        printf("Not a valid integer. Try again.\n");
    }
}
float prompt_float(const char *label){
    char buf[128];
    while(1){
        printf("%s", label);
        if(!fgets(buf,sizeof(buf),stdin)) return 0.0f;
        rstrip(buf); trim(buf);
        char *end=NULL; float v=(float)strtod(buf,&end);
        if(end && (*end=='\0')) return v;
        printf("Not a valid number. Try again (e.g., 88.5).\n");
    }
}
/* Edit helpers (blank = keep current) */
void prompt_edit_string(const char *label, const char *current, char *out, size_t outsz){
    printf("%s (current: %s) -> ", label, current);
    if(!fgets(out,(int)outsz,stdin)){ out[0]='\0'; return; }
    rstrip(out); trim(out);
}
float prompt_edit_float(const char *label, float current){
    char buf[128];
    printf("%s (current: %.1f) -> ", label, current);
    if(!fgets(buf,sizeof(buf),stdin)) return current;
    rstrip(buf); trim(buf);
    if(buf[0]=='\0') return current;
    char *end=NULL; float v=(float)strtod(buf,&end);
    if(end && *end=='\0') return v;
    printf("Not a valid number, keeping current.\n");
    return current;
}

/* ---------- Parse KEY=VALUE (quotes ok, spaces around '=' ok) ---------- */
int parse_kv(const char *src, const char *KEY, char *out, size_t outsz){
    size_t klen=strlen(KEY); const char *p=src;
    while(*p){
        while(*p && isspace((unsigned char)*p)) p++;
        const char *q=p; size_t i=0;
        while(i<klen && q[i] && toupper((unsigned char)q[i])==toupper((unsigned char)KEY[i])) i++;
        if(i==klen){
            q+=i; while(*q && isspace((unsigned char)*q)) q++;
            if(*q=='='){
                q++; while(*q && isspace((unsigned char)*q)) q++;
                if(*q=='"'){ q++; const char *e=q; while(*e && *e!='"') e++; size_t len=(size_t)(e-q);
                    if(len>=outsz) len=outsz-1; strncpy(out,q,len); out[len]='\0'; return 1;
                }else{ const char *e=q; while(*e && !isspace((unsigned char)*e)) e++; size_t len=(size_t)(e-q);
                    if(len>=outsz) len=outsz-1; strncpy(out,q,len); out[len]='\0'; return 1;
                }
            }
        }
        while(*p && !isspace((unsigned char)*p)) p++;
        while(*p && isspace((unsigned char)*p)) p++;
    }
    return 0;
}

/* ===================== FILE I/O ===================== */
/* Reads typical CMS files with tabs *or* aligned spaces. */
int load_from_file(const char *filename){
    FILE *fp=fopen(filename,"r"); if(!fp) return 0;
    g_count=0; char line[LINE_MAX_LEN]; int table_started=0;

    while(fgets(line,sizeof(line),fp)){
        rstrip(line);
        char raw[LINE_MAX_LEN]; strncpy(raw,line,LINE_MAX_LEN-1); raw[LINE_MAX_LEN-1]=0; trim(raw);
        if(raw[0]=='\0') continue;

        if(!table_started){
            char up[LINE_MAX_LEN]; strncpy(up,raw,LINE_MAX_LEN-1); up[LINE_MAX_LEN-1]=0;
            for(char *u=up; *u; ++u) *u=(char)toupper((unsigned char)*u);
            if(strstr(up,"ID") && strstr(up,"MARK")) table_started=1;
            continue;
        }
        if(!isdigit((unsigned char)raw[0])) continue;  // data rows start with ID

        char tmp[LINE_MAX_LEN]; strncpy(tmp,raw,LINE_MAX_LEN-1); tmp[LINE_MAX_LEN-1]=0;
        normalize_delims(tmp);

        char *idtok=strtok(tmp,"\t");
        char *nametok=strtok(NULL,"\t");
        char *progtok=strtok(NULL,"\t");
        char *marktok=strtok(NULL,"\t");

        if(idtok && nametok && progtok && marktok){
            trim(idtok); trim(nametok); trim(progtok); trim(marktok);
            if(g_count<MAX_STUDENTS){
                Student s; s.id=atoi(idtok);
                strncpy(s.name,nametok,NAME_MAX_LEN-1); s.name[NAME_MAX_LEN-1]=0;
                strncpy(s.programme,progtok,PROG_MAX_LEN-1); s.programme[PROG_MAX_LEN-1]=0;
                s.mark=(float)atof(marktok?marktok:"0"); // safety
                g_students[g_count++]=s;
            }
            continue;
        }

        /* Fallback: split manually -> first token = ID, last token = Mark,
           middle strictly between end-of-ID and start-of-Mark. */
        char work[LINE_MAX_LEN]; strncpy(work,raw,LINE_MAX_LEN-1); work[LINE_MAX_LEN-1]=0;

        char *p=work; while(*p && isspace((unsigned char)*p)) p++;
        char *id_start=p; while(*p && !isspace((unsigned char)*p)) p++;
        char hold=*p; *p='\0'; int id=atoi(id_start); *p=hold;

        char *after_id=p; while(*after_id && isspace((unsigned char)*after_id)) after_id++;

        char *end=work+strlen(work)-1;
        while(end>work && isspace((unsigned char)*end)) *end--='\0';
        char *mark_start=end; while(mark_start>work && !isspace((unsigned char)mark_start[-1])) mark_start--;
        float mark=(float)atof(mark_start);
        if(mark_start>work){ char *cut=mark_start-1; while(cut>work && isspace((unsigned char)*cut)) *cut--='\0'; }

        char middle[LINE_MAX_LEN];
        size_t midlen=(size_t)(mark_start-after_id);
        if(midlen>=sizeof(middle)) midlen=sizeof(middle)-1;
        strncpy(middle,after_id,midlen); middle[midlen]='\0';
        trim(middle); normalize_delims(middle);

        char *name2=strtok(middle,"\t");
        char *prog2=strtok(NULL,"\t");

        if(id>0 && name2 && prog2 && g_count<MAX_STUDENTS){
            Student s; s.id=id;
            strncpy(s.name,name2,NAME_MAX_LEN-1); s.name[NAME_MAX_LEN-1]=0; trim(s.name);
            strncpy(s.programme,prog2,PROG_MAX_LEN-1); s.programme[PROG_MAX_LEN-1]=0; trim(s.programme);
            s.mark=mark; g_students[g_count++]=s;
        }
    }
    fclose(fp); return 1;
}

int save_to_file(const char *filename){
    FILE *fp=fopen(filename,"w"); if(!fp) return 0;
    fprintf(fp,"Database Name: StudentRecords\nAuthors: Team\n\n");
    fprintf(fp,"Table Name: StudentRecords\n");
    fprintf(fp,"ID\tName\tProgramme\tMark\n");
    for(int i=0;i<g_count;++i)
        fprintf(fp,"%d\t%s\t%s\t%.1f\n",
                g_students[i].id,
                g_students[i].name,
                g_students[i].programme,
                g_students[i].mark);
    fclose(fp); return 1;
}

/* ===================== COMMANDS ===================== */
void show_help(void){
    printf("\nAvailable Commands:\n");
    printf("  OPEN <filename>   -> open the database file and read in all records\n");
    printf("  SHOW ALL          -> display all current records in memory\n");
    printf("  INSERT            -> insert a new record (prompts every column)\n");
    printf("  QUERY ID=<n>      -> search for a record with a given student ID\n");
    printf("  UPDATE ID=<n>     -> update the data (prompts every column; Enter keeps)\n");
    printf("  DELETE ID=<n>     -> delete the record (double confirm)\n");
    printf("  SAVE              -> save all current records into the database file\n");
    printf("  SHOW SUMMARY      -> show total students, average mark,\n");
    printf("                       highest and lowest mark (with names)\n");
    printf("  HELP / EXIT       -> help or quit\n\n");
}

/* OPEN */
void cmd_open(const char *args){
    char fname[260]="";
    while(*args && isspace((unsigned char)*args)) args++;
    size_t i=0; while(*args && !isspace((unsigned char)*args) && i<sizeof(fname)-1) fname[i++]=*args++;
    fname[i]=0;
    if(fname[0]=='\0'){ printf("CMS: Please provide a filename. Example: OPEN Sample-CMS.txt\n"); return; }
    if(!load_from_file(fname)){
        strncpy(g_open_filename,fname,sizeof(g_open_filename)-1);
        printf("CMS: File not found. A new one will be created on SAVE.\n");
        return;
    }
    strncpy(g_open_filename,fname,sizeof(g_open_filename)-1);
    printf("CMS: The database file \"%s\" is successfully opened. (%d records loaded)\n", fname, g_count);
}

/* SHOW ALL */
/* SHOW ALL — neatly formatted columns */
void cmd_show_all(void) {
    if (g_count == 0) {
        printf("CMS: No records loaded.\n");
        return;
    }
    // table header
    printf("%-10s %-20s %-25s %-6s\n", "ID", "Name", "Programme", "Mark");
    // table rows
    for (int i = 0; i < g_count; ++i) {
        printf("%-10d %-20s %-25s %-6.1f\n",
               g_students[i].id,
               g_students[i].name,
               g_students[i].programme,
               g_students[i].mark);
    }
}

/* INSERT (exact spec) */
void cmd_insert(const char *args){
    if(g_count>=MAX_STUDENTS){ printf("CMS: Storage is full.\n"); return; }

    char buf[256]; int id;
    if(parse_kv(args,"ID",buf,sizeof(buf))) id=atoi(buf);
    else id=prompt_int("Enter student ID: ");

    if(find_index_by_id(id)>=0){
        printf("CMS: A record with the same student ID already exists. Insertion cancelled.\n");
        return;
    }

    char name[NAME_MAX_LEN], prog[PROG_MAX_LEN];
    float mark;

    if(parse_kv(args,"NAME",buf,sizeof(buf))){ strncpy(name,buf,NAME_MAX_LEN-1); name[NAME_MAX_LEN-1]=0; }
    else prompt_string("Enter Name: ", name, sizeof(name));

    if(parse_kv(args,"PROGRAMME",buf,sizeof(buf))){ strncpy(prog,buf,PROG_MAX_LEN-1); prog[PROG_MAX_LEN-1]=0; }
    else prompt_string("Enter Programme: ", prog, sizeof(prog));

    if(parse_kv(args,"MARK",buf,sizeof(buf))) mark=(float)atof(buf);
    else mark=prompt_float("Enter Mark (e.g., 88.5): ");

    Student s; s.id=id;
    strncpy(s.name,name,NAME_MAX_LEN-1); s.name[NAME_MAX_LEN-1]=0;
    strncpy(s.programme,prog,PROG_MAX_LEN-1); s.programme[PROG_MAX_LEN-1]=0;
    s.mark=mark;
    g_students[g_count++]=s;
    printf("CMS: New record inserted.\n");
}

/* QUERY */
void cmd_query(const char *args) {
    char buf[64];
    int id;

    // if user didn’t type ID=..., ask them
    if (parse_kv(args, "ID", buf, sizeof(buf))) {
        id = atoi(buf);
    } else {
        id = prompt_int("Enter student ID to search: ");
    }

    // find the record
    int idx = find_index_by_id(id);
    if (idx < 0) {
        printf("CMS: No record found with the same student ID.\n");
        return;
    }

    // display result
    Student *s = &g_students[idx];
    printf("CMS: The record with ID=%d is found.\n", id);
    printf("ID\tName\tProgramme\tMark\n");
    printf("%d\t%s\t%s\t%.1f\n", s->id, s->name, s->programme, s->mark);
}

/* UPDATE (prompt every column; Enter = keep) */
void cmd_update(const char *args) {
    char buf[256];
    int id;

    /* ID: from args if present, otherwise ask */
    if (parse_kv(args, "ID", buf, sizeof(buf))) {
        id = atoi(buf);
    } else {
        id = prompt_int("Enter student ID to update: ");
    }

    int idx = find_index_by_id(id);
    if (idx < 0) {
        printf("CMS: No record found with the same student ID.\n");
        return;
    }

    Student *s = &g_students[idx];
    int changed = 0;

    /* -------- Name -------- */
    if (parse_kv(args, "NAME", buf, sizeof(buf))) {
        strncpy(s->name, buf, NAME_MAX_LEN - 1);
        s->name[NAME_MAX_LEN - 1] = 0;
        changed = 1;
    } else {
        char newName[NAME_MAX_LEN] = "";
        prompt_edit_string("Enter new Name", s->name, newName, sizeof(newName));
        if (newName[0]) {
            strncpy(s->name, newName, NAME_MAX_LEN - 1);
            s->name[NAME_MAX_LEN - 1] = 0;
            changed = 1;
        }
    }

    /* -------- Programme -------- */
    if (parse_kv(args, "PROGRAMME", buf, sizeof(buf))) {
        strncpy(s->programme, buf, PROG_MAX_LEN - 1);
        s->programme[PROG_MAX_LEN - 1] = 0;
        changed = 1;
    } else {
        char newProg[PROG_MAX_LEN] = "";
        prompt_edit_string("Enter new Programme", s->programme, newProg, sizeof(newProg));
        if (newProg[0]) {
            strncpy(s->programme, newProg, PROG_MAX_LEN - 1);
            s->programme[PROG_MAX_LEN - 1] = 0;
            changed = 1;
        }
    }

    /* -------- Mark -------- */
    if (parse_kv(args, "MARK", buf, sizeof(buf))) {
        s->mark = (float)atof(buf);
        changed = 1;
    } else {
        float newMark = prompt_edit_float("Enter new Mark", s->mark);
        if (newMark != s->mark) {
            s->mark = newMark;
            changed = 1;
        }
    }

    if (changed) printf("CMS: Record updated.\n");
    else         printf("CMS: No fields changed.\n");
}

/* DELETE (double confirm) */
/* DELETE — same style as INSERT + matches requirement (double confirm) */
void cmd_delete(const char *args) {
    char buf[64];
    int id;

    /* ID: use from args if given, otherwise prompt */
    if (parse_kv(args, "ID", buf, sizeof(buf))) {
        id = atoi(buf);
    } else {
        id = prompt_int("Enter student ID to delete: ");
    }

    /* If no record with same student ID -> warning message */
    int idx = find_index_by_id(id);
    if (idx < 0) {
        printf("CMS: No record found with the same student ID.\n");
        return;
    }

    /* (Optional) show the record we’re about to delete */
    Student *s = &g_students[idx];
    printf("\nAbout to delete this record:\n");
    printf("ID\tName\tProgramme\tMark\n");
    printf("%d\t%s\t%s\t%.1f\n\n", s->id, s->name, s->programme, s->mark);

    /* Double confirmation */
    printf("Confirm deletion? (Y/N): ");
    char a1[8]; if (!fgets(a1, sizeof(a1), stdin)) return; rstrip(a1);
    if (toupper((unsigned char)a1[0]) != 'Y') {
        printf("CMS: Deletion cancelled.\n");
        return;
    }

    printf("Please type Y again to confirm deletion: ");
    char a2[8]; if (!fgets(a2, sizeof(a2), stdin)) return; rstrip(a2);
    if (toupper((unsigned char)a2[0]) != 'Y') {
        printf("CMS: Deletion cancelled.\n");
        return;
    }

    /* Do the delete (shift left) */
    for (int i = idx + 1; i < g_count; ++i) g_students[i - 1] = g_students[i];
    g_count--;

    printf("CMS: Record deleted.\n");
}

/* SAVE */
void cmd_save(void){
    if(g_open_filename[0]=='\0'){ printf("CMS: No file open.\n"); return; }
    if(save_to_file(g_open_filename))
        printf("CMS: Saved successfully to \"%s\".\n", g_open_filename);
    else
        printf("CMS: Failed to save.\n");
}
/* SHOW SUMMARY */
static void cmd_show_summary(void) {
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

/* ===================== UI / MAIN ===================== */
void print_declaration(void) {
    FILE *fp = fopen("declaration.txt", "r");
    if (!fp) {
        printf("Error: declaration.txt not found.\n\n");
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }

    fclose(fp);
    printf("\n");
}

int main(void){
    print_declaration();
    show_help();

    char line[LINE_MAX_LEN];
    while(1){
        printf("> ");
        if(!fgets(line,sizeof(line),stdin)) break;
        rstrip(line); if(line[0]=='\0') continue;

        char cmd[64]={0}; const char *p=line; while(*p && isspace((unsigned char)*p)) p++;
        size_t ci=0; while(*p && !isspace((unsigned char)*p) && ci<sizeof(cmd)-1) cmd[ci++]=*p++;
        while(*p && isspace((unsigned char)*p)) p++;

        if(equals_ic(cmd,"EXIT")) break;
        else if(equals_ic(cmd,"HELP")) show_help();
        else if(equals_ic(cmd,"OPEN")) cmd_open(p);
        else if (equals_ic(cmd, "SHOW")) {
    if (equals_ic(p, "ALL")) {
        cmd_show_all();
    } else if (equals_ic(p, "SUMMARY")) {
        cmd_show_summary();     // <-- new summary function you wrote
    } else {
        printf("CMS: Use SHOW ALL or SHOW SUMMARY.\n");
    }
}
        else if(equals_ic(cmd,"INSERT")) cmd_insert(p);
        else if(equals_ic(cmd,"QUERY"))  cmd_query(p);
        else if(equals_ic(cmd,"UPDATE")) cmd_update(p);
        else if(equals_ic(cmd,"DELETE")) cmd_delete(p);
        else if(equals_ic(cmd,"SAVE"))   cmd_save();
        else printf("CMS: Unknown command. Type HELP for the menu.\n");
    }
    return 0;
}
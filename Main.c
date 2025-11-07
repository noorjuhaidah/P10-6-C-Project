// Main.c
// INF1002 Programming Fundamentals - Class Management System (Basic Version)
// Compile:  gcc Main.c -o CMS
// Run:      ./CMS

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_CAPACITY 64
#define NAME_MAX_LEN     128
#define PROG_MAX_LEN     128
#define LINE_MAX_LEN     1024

typedef struct {
    int   id;
    char  name[NAME_MAX_LEN];
    char  programme[PROG_MAX_LEN];
    float mark;
} Student;

typedef struct {
    Student *data;
    size_t   size;
    size_t   cap;
} StudentVec;

static StudentVec g_students = {NULL, 0, 0};
static char g_open_filename[260] = "";

// ---------- Utilities ----------
static void rstrip(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}
static void trim(char *s) {
    size_t start = 0;
    while (isspace((unsigned char)s[start])) start++;
    if (start) memmove(s, s+start, strlen(s+start)+1);
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = '\0';
}
static void strtoupper(char *s) {
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

// Dynamic vector
static void vec_init(StudentVec *v) {
    v->cap = INITIAL_CAPACITY;
    v->size = 0;
    v->data = malloc(v->cap * sizeof(Student));
}
static void vec_free(StudentVec *v) {
    free(v->data); v->data=NULL; v->size=0; v->cap=0;
}
static void vec_clear(StudentVec *v) { v->size = 0; }
static void vec_push(StudentVec *v, Student s) {
    if (v->size >= v->cap) {
        v->cap *= 2;
        v->data = realloc(v->data, v->cap * sizeof(Student));
    }
    v->data[v->size++] = s;
}
static int find_index_by_id(const StudentVec *v, int id) {
    for (size_t i=0;i<v->size;i++) if (v->data[i].id==id) return (int)i;
    return -1;
}

// collapse runs of >=2 spaces or tabs into a single '\t' delimiter
static void normalize_delims(char *s) {
    char buf[LINE_MAX_LEN]; size_t bi=0; int space_run=0;
    for (size_t i=0; s[i] && bi+1<sizeof(buf); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c=='\t') { buf[bi++]='\t'; space_run=0; continue; }
        if (c==' ') space_run++;
        else {
            if (space_run>=2) buf[bi++]='\t';
            else if (space_run==1) buf[bi++]=' ';
            space_run=0;
            buf[bi++]=(char)c;
        }
    }
    if (space_run>=2) buf[bi++]='\t';
    else if (space_run==1) buf[bi++]=' ';
    buf[bi]='\0';
    strncpy(s,buf,LINE_MAX_LEN-1); s[LINE_MAX_LEN-1]=0;
}

// Portable strtok wrapper
#if defined(_MSC_VER)
  #define TOK(str,delim,saveptr) strtok_s((str),(delim),(saveptr))
#else
  #define TOK(str,delim,saveptr) strtok_r((str),(delim),(saveptr))
#endif

// parse KEY=VALUE (quoted or not)
static int parse_kv(const char *src,const char *KEY,char *out,size_t outsz){
    char upkey[64]; strncpy(upkey,KEY,63); upkey[63]=0; strtoupper(upkey);
    const char *p=src;
    while(*p){
        while(*p && isspace((unsigned char)*p)) p++;
        const char *t=p; int inq=0; const char *eq=NULL;
        while(*p){ if(*p=='"') inq=!inq; if(!inq && isspace((unsigned char)*p)) break; if(!eq && *p=='=') eq=p; p++; }
        if(eq && eq>t){
            size_t keylen=eq-t; char keybuf[64];
            if(keylen>63) keylen=63; strncpy(keybuf,t,keylen); keybuf[keylen]=0;
            trim(keybuf); strtoupper(keybuf);
            if(strcmp(keybuf,upkey)==0){
                const char *v=eq+1; while(*v && isspace((unsigned char)*v)) v++;
                if(*v=='"'){ v++; const char *vend=v; while(*vend && *vend!='"') vend++; size_t len=vend-v; if(len>=outsz) len=outsz-1; strncpy(out,v,len); out[len]=0; return 1;}
                else { const char *vend=v; while(*vend && !isspace((unsigned char)*vend)) vend++; size_t len=vend-v; if(len>=outsz) len=outsz-1; strncpy(out,v,len); out[len]=0; return 1;}
            }
        }
    }
    return 0;
}

// ---------- File I/O ----------
static int load_from_file(const char *filename){
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    vec_clear(&g_students);

    char line[LINE_MAX_LEN];
    int table_started = 0;

    while (fgets(line, sizeof(line), fp)) {
        rstrip(line);

        char raw[LINE_MAX_LEN];
        strncpy(raw, line, LINE_MAX_LEN-1); raw[LINE_MAX_LEN-1] = 0;
        trim(raw);
        if (raw[0] == '\0') continue;

        if (!table_started) {
            char up[LINE_MAX_LEN];
            strncpy(up, raw, LINE_MAX_LEN-1); up[LINE_MAX_LEN-1] = 0;
            strtoupper(up);
            if (strstr(up, "ID") && strstr(up, "MARK")) table_started = 1;
            continue;
        }

        if (!isdigit((unsigned char)raw[0])) continue;

        // normalize to tabs
        char tmp[LINE_MAX_LEN];
        strncpy(tmp, raw, LINE_MAX_LEN-1); tmp[LINE_MAX_LEN-1]=0;
        normalize_delims(tmp);

        char *save=NULL;
        char *idtok=TOK(tmp,"\t",&save);
        char *nametok=TOK(NULL,"\t",&save);
        char *progtok=TOK(NULL,"\t",&save);
        char *marktok=TOK(NULL,"\t",&save);

        if (idtok && nametok && progtok && marktok) {
            Student s; memset(&s,0,sizeof(s));
            s.id=atoi(idtok);
            strncpy(s.name,nametok,NAME_MAX_LEN-1); trim(s.name);
            strncpy(s.programme,progtok,PROG_MAX_LEN-1); trim(s.programme);
            s.mark=(float)atof(marktok);
            vec_push(&g_students,s);
            continue;
        }

        // fallback: first token = ID, last = mark
        char work[LINE_MAX_LEN];
        strncpy(work, raw, LINE_MAX_LEN-1); work[LINE_MAX_LEN-1]=0;
        char *p=work;
        while(*p && isspace((unsigned char)*p)) p++;
        char *id_start=p;
        while(*p && !isspace((unsigned char)*p)) p++;
        char hold=*p; *p='\0';
        int id=atoi(id_start);
        *p=hold;
        char *end=work+strlen(work)-1;
        while(end>p && isspace((unsigned char)*end)) *end--='\0';
        char *mark_start=end;
        while(mark_start>work && !isspace((unsigned char)mark_start[-1])) mark_start--;
        float mark=(float)atof(mark_start);
        if(mark_start>work){ char *cut=mark_start-1; while(cut>work && isspace((unsigned char)*cut)) *cut--='\0'; }
        char middle[LINE_MAX_LEN];
        size_t midlen=(size_t)(mark_start-work);
        if(midlen>=sizeof(middle)) midlen=sizeof(middle)-1;
        strncpy(middle,work,midlen); middle[midlen]='\0';
        trim(middle);
        normalize_delims(middle);
        char *sv2=NULL;
        char *name2=TOK(middle,"\t",&sv2);
        char *prog2=TOK(NULL,"\t",&sv2);
        Student s; memset(&s,0,sizeof(s));
        s.id=id;
        if(name2) {strncpy(s.name,name2,NAME_MAX_LEN-1); trim(s.name);}
        if(prog2){strncpy(s.programme,prog2,PROG_MAX_LEN-1); trim(s.programme);}
        s.mark=mark;
        if(s.id>0 && s.name[0] && s.programme[0]) vec_push(&g_students,s);
    }

    fclose(fp);
    return 1;
}

static int save_to_file(const char *filename){
    FILE *fp=fopen(filename,"w"); if(!fp) return 0;
    fprintf(fp,"Database Name: StudentRecords\nAuthors: Team\n\nTable Name: StudentRecords\n");
    fprintf(fp,"ID\tName\tProgramme\tMark\n");
    for(size_t i=0;i<g_students.size;i++){
        Student *s=&g_students.data[i];
        fprintf(fp,"%d\t%s\t%s\t%.1f\n",s->id,s->name,s->programme,s->mark);
    }
    fclose(fp); return 1;
}

// ---------- Commands ----------
static void cmd_open(const char *args){
    char fname[260]="";
    while(*args && isspace((unsigned char)*args)) args++;
    size_t i=0; while(*args && !isspace((unsigned char)*args) && i<sizeof(fname)-1) fname[i++]=*args++;
    fname[i]=0;
    if(fname[0]=='\0'){ printf("CMS: Please provide a filename. Example: OPEN P10_6-CMS.txt\n"); return; }
    if(!load_from_file(fname)){
        strncpy(g_open_filename,fname,259);
        printf("CMS: The database file \"%s\" was not found. A new one will be created on SAVE.\n",fname); return;
    }
    strncpy(g_open_filename,fname,259);
    printf("CMS: The database file \"%s\" is successfully opened. (%zu records loaded)\n",fname,g_students.size);
}
static void show_all(void){
    if(g_students.size==0){ printf("CMS: No records loaded.\n"); return; }
    printf("CMS: Here are all the records found in the table \"StudentRecords\".\n");
    printf("ID  Name  Programme  Mark\n");
    for(size_t i=0;i<g_students.size;i++){
        Student *s=&g_students.data[i];
        printf("%d %s %s %.1f\n",s->id,s->name,s->programme,s->mark);
    }
}
static void cmd_insert(const char *args){
    char buf[256];
    if(!parse_kv(args,"ID",buf,sizeof(buf))){ printf("CMS: Missing ID.\n"); return; }
    int id=atoi(buf);
    if(find_index_by_id(&g_students,id)>=0){ printf("CMS: The record with ID=%d already exists.\n",id); return; }
    char name[NAME_MAX_LEN],prog[PROG_MAX_LEN];
    if(!parse_kv(args,"NAME",buf,sizeof(buf))){ printf("CMS: Missing Name.\n"); return; }
    strncpy(name,buf,NAME_MAX_LEN-1);
    if(!parse_kv(args,"PROGRAMME",buf,sizeof(buf))){ printf("CMS: Missing Programme.\n"); return; }
    strncpy(prog,buf,PROG_MAX_LEN-1);
    if(!parse_kv(args,"MARK",buf,sizeof(buf))){ printf("CMS: Missing Mark.\n"); return; }
    float mark=(float)atof(buf);
    Student s={id,"","",mark}; strncpy(s.name,name,NAME_MAX_LEN-1); strncpy(s.programme,prog,PROG_MAX_LEN-1);
    vec_push(&g_students,s);
    printf("CMS: A new record with ID=%d is successfully inserted.\n",id);
}
static void cmd_query(const char *args){
    char buf[64];
    if(!parse_kv(args,"ID",buf,sizeof(buf))){ printf("CMS: Missing ID.\n"); return; }
    int id=atoi(buf); int idx=find_index_by_id(&g_students,id);
    if(idx<0){ printf("CMS: The record with ID=%d does not exist.\n",id); return; }
    Student *s=&g_students.data[idx];
    printf("CMS: The record with ID=%d is found.\n",id);
    printf("ID  Name  Programme  Mark\n%d %s %s %.1f\n",s->id,s->name,s->programme,s->mark);
}
static void cmd_update(const char *args){
    char buf[128];
    if(!parse_kv(args,"ID",buf,sizeof(buf))){ printf("CMS: Missing ID.\n"); return; }
    int id=atoi(buf); int idx=find_index_by_id(&g_students,id);
    if(idx<0){ printf("CMS: The record with ID=%d does not exist.\n",id); return; }
    Student *s=&g_students.data[idx]; int changed=0;
    if(parse_kv(args,"NAME",buf,sizeof(buf))){ strncpy(s->name,buf,NAME_MAX_LEN-1); changed=1; }
    if(parse_kv(args,"PROGRAMME",buf,sizeof(buf))){ strncpy(s->programme,buf,PROG_MAX_LEN-1); changed=1; }
    if(parse_kv(args,"MARK",buf,sizeof(buf))){ s->mark=(float)atof(buf); changed=1; }
    printf(changed?"CMS: The record with ID=%d is successfully updated.\n":"CMS: No fields updated.\n",id);
}
static void cmd_delete(const char *args){
    char buf[64];
    if(!parse_kv(args,"ID",buf,sizeof(buf))){ printf("CMS: Missing ID.\n"); return; }
    int id=atoi(buf); int idx=find_index_by_id(&g_students,id);
    if(idx<0){ printf("CMS: The record with ID=%d does not exist.\n",id); return; }
    printf("CMS: Are you sure you want to delete ID=%d? Type Y to confirm: ",id);
    char ans[8]; if(!fgets(ans,sizeof(ans),stdin)) return; rstrip(ans);
    if(toupper((unsigned char)ans[0])=='Y'){ for(size_t i=idx+1;i<g_students.size;i++) g_students.data[i-1]=g_students.data[i]; g_students.size--; printf("CMS: Record deleted.\n"); }
    else printf("CMS: Deletion cancelled.\n");
}
static void cmd_save(void){
    if(g_open_filename[0]=='\0'){ printf("CMS: No file open.\n"); return; }
    if(save_to_file(g_open_filename)) printf("CMS: The database file \"%s\" is successfully saved.\n",g_open_filename);
    else printf("CMS: Failed to save file.\n");
}

// ---------- Declaration ----------
static void print_declaration(void){
    printf("Declaration\n");
    printf("SIT’s policy on copying does not allow students to copy source code or assessment solutions from others.\n");
    printf("We hereby declare that we understand and agree to this policy.\n\n");
}

// ---------- Main ----------
int main(void){
    vec_init(&g_students);
    print_declaration();
    printf("Commands: OPEN, SHOW ALL, INSERT, QUERY, UPDATE, DELETE, SAVE, EXIT\n");
    char line[LINE_MAX_LEN];
    while(1){
        printf("\n> ");
        if(!fgets(line,sizeof(line),stdin)) break;
        rstrip(line); if(line[0]=='\0') continue;
        char cmd[64]; const char *args; size_t i=0; const char *p=line;
        while(*p && isspace((unsigned char)*p)) p++;
        while(*p && !isspace((unsigned char)*p) && i<63) cmd[i++]=*p++;
        cmd[i]=0; while(*p && isspace((unsigned char)*p)) p++; args=p;
        char up[64]; strncpy(up,cmd,63); up[63]=0; strtoupper(up);
        if(strcmp(up,"EXIT")==0) break;
        else if(strcmp(up,"OPEN")==0) cmd_open(args);
        else if(strcmp(up,"SHOW")==0){ if(strncasecmp(args,"ALL",3)==0) show_all(); else printf("CMS: Use SHOW ALL.\n"); }
        else if(strcmp(up,"INSERT")==0) cmd_insert(args);
        else if(strcmp(up,"QUERY")==0) cmd_query(args);
        else if(strcmp(up,"UPDATE")==0) cmd_update(args);
        else if(strcmp(up,"DELETE")==0) cmd_delete(args);
        else if(strcmp(up,"SAVE")==0) cmd_save();
        else printf("CMS: Unknown command.\n");
    }
    vec_free(&g_students);
    return 0;
}

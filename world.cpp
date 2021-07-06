//
// Created by Selin Yıldırım on 8.04.2021.
//
#include "logging.c"
#include <vector>
#include <unistd.h>
#include<iostream>
using namespace std;

#include<sys/types.h>
#include<sys/socket.h>
#include <signal.h>
#include<sys/wait.h>
#include<string>
#include <cstring>
#include <limits.h>
#include <bits/stdc++.h>
#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, PF_UNIX, fd)

int width=0, height=0, door_x=0, door_y=0, player_x=0, player_y=0;
int sockets[2], num_monsters=0;
char  player_distance[PATH_MAX], player_attack_count[PATH_MAX], num_turns[PATH_MAX];
pid_t pid_player,pid_game;
int player_status,  monster_status, t_damage_on_player=0;
game_over_status go;
char playerExecName[PATH_MAX];

typedef struct monster { // this is only for convenience, a different structure than the one in monster.cpp
    coordinate position;
    char health[PATH_MAX];
    char name;
    char execMonsterName[PATH_MAX];
    char range[PATH_MAX];
    char damage_val[PATH_MAX];
    char defence_val[PATH_MAX];
    int socket_val_gw;
    pid_t m_pid;
} monster;

vector<monster> monsters;
map_info world_map;
int flag=0;
char buf[1024], buf2[1024];

void initialize_inp(){
    scanf(" %d %d %d %d %d %d\n", &width, &height, &door_x, &door_y, &player_x, &player_y);
    scanf(" %s \n", playerExecName);
    cin >> player_attack_count >>  player_distance >> num_turns;
    scanf(" %d\n", &num_monsters);
    if(!num_monsters) {exit(-1);}
    for(int i=0; i<num_monsters;  i++){
        monster temp;
        if (num_monsters-1!=i) scanf(" %s %c %d %d %s %s %s %s\n", temp.execMonsterName, &temp.name, &temp.position.x, &temp.position.y, temp.health, temp.damage_val, temp.defence_val, temp.range);
        if (num_monsters-1==i) scanf(" %s %c %d %d %s %s %s %s",  temp.execMonsterName,  &temp.name, &temp.position.x, &temp.position.y, temp.health, temp.damage_val, temp.defence_val, temp.range);
        monsters.push_back(temp);
        coordinate temp2;
        temp2.x = temp.position.x;
        temp2.y = temp.position.y;
        world_map.monster_coordinates[i]= temp2;
        world_map.monster_types[i]=temp.name;
    }
    world_map.alive_monster_count=num_monsters;
    world_map.door.x=door_x, world_map.door.y=door_y;
    world_map.map_height=height, world_map.map_width=width;
    world_map.player.x=player_x, world_map.player.y=player_y;
}

int send_player_message(int p_x, int p_y,int al_m_count, int tot_damg, bool gm_ov, coordinate* mons_coordins){
    player_message a;
    a.new_position.x = p_x, a.new_position.y=p_y, a.alive_monster_count=al_m_count;
    a.total_damage=tot_damg, a.game_over=gm_ov;
    for(int i=0; i< al_m_count; i++) a.monster_coordinates[i]=mons_coordins[i];
    char *byteStream = (char *) &a; // serialize message
    if (write(sockets[0], byteStream, sizeof(player_message)) < 0) return -1;
    else return 0;
}
player_response* receive_player_response(){
    int a = read(sockets[0], buf, sizeof(player_response)+1) ;
    if(a <= 0) return NULL;
    player_response *received_p = (player_response *) buf;
    return received_p;
}
int send_monster_message(int m_socket, int m_x, int m_y, int tot_damg, int p_x, int p_y, bool g_ov){
    monster_message a;
    a.new_position.x = m_x, a.new_position.y=m_y, a.player_coordinate.x=p_x, a.player_coordinate.y=p_y;
    a.damage=tot_damg, a.game_over=g_ov;
    char *byteStream = (char *) &a;
    if (write(m_socket, byteStream, sizeof(monster_message)) < 0) return -1;
    else return 0;
}
void gameover_messages(bool player_already_dead){
    if(!player_already_dead) { // do not send message to player as she is already killed by monsters :)
        send_player_message(-1, -1, world_map.alive_monster_count, 0, 1, world_map.monster_coordinates);
    }
    waitpid(pid_player, &player_status, WNOHANG);
    //if (WIFEXITED(player_status)) cout << "go and player died normally." << endl;
    //else cout << "go but player exitted abnormally." << endl;
    close(sockets[0]); // close game world's player's side socket
    for(int i=0; i<world_map.alive_monster_count; i++){
        send_monster_message(monsters[i].socket_val_gw,  -1,  -1,  -1,  -1,  -1,  1);
        waitpid(pid_player, &monster_status, WNOHANG);
        //if(WIFEXITED(monster_status)) cout << "go and monster index" << i << " died normally." << endl;
        //else cout << "go but monster index" << i << " died abnormally." << endl;
        close(monsters[i].socket_val_gw);
    }

}
int process_move(bool monster, player_response* request, monster_response* m_request){
    int new_x, new_y;
    if(!monster){
        new_x=request->pr_content.move_to.x;
        new_y=request->pr_content.move_to.y;

        if((new_x==world_map.door.x) && (new_y==world_map.door.y)){  // player on the door
            world_map.player.x=new_x; world_map.player.y=new_y; go=go_reached; return 2;
        }
    }
    else{
        new_x=m_request->mr_content.move_to.x;
        new_y=m_request->mr_content.move_to.y;
    }

    bool cond1 = (1<=new_x && new_x<=width-2) && (1<=new_y && new_y<=height-2);
    bool cond2 = 1;
    for(int i=0; i<world_map.alive_monster_count; i++){
        if((monsters[i].position.x==new_x) && (monsters[i].position.y==new_y)) {cond2=0; break;}
    }
    if((monster) && (world_map.player.x==new_x) && (world_map.player.y==new_y)) {cond2=0;}
    if((!cond1) || (!cond2)) return 0;
    else{
        if(!monster) { world_map.player.x=new_x; world_map.player.y=new_y;}
    }
    return 1; // both monster and player obtains acceptance here
}

bool sortby_x(const monster &a, const monster &b) { return (a.position.x < b.position.x); }
bool sortby_y(const monster &a, const monster &b) { return (a.position.y < b.position.y); }
bool sortby_x_arr(const coordinate &a, const coordinate &b) { return (a.x < b.x); }
bool sortby_y_arr(const coordinate &a, const coordinate &b) { return (a.y < b.y); }
void sort_monsters_and_game_world_monsters(){
    //sort(monsters.begin(), monsters.end(), sortby_x); // my custom sort function
    //sort(monsters.begin(), monsters.end(), sortby_y);
    stable_sort(monsters.begin(), monsters.end(), sortby_y);
    stable_sort(monsters.begin(), monsters.end(), sortby_x);
    stable_sort(world_map.monster_coordinates, world_map.monster_coordinates+num_monsters, sortby_y_arr);
    stable_sort(world_map.monster_coordinates, world_map.monster_coordinates+num_monsters, sortby_x_arr);

    for (int i=0; i < world_map.alive_monster_count; i++) {
        world_map.monster_types[i]=monsters[i].name; // order the names in game world
    }
}

int receive_monster_responses(){ // IMPLEMENT the game over condition !!
    int i=0, deleted=0, temp=world_map.alive_monster_count;
    t_damage_on_player = 0;
    for(; i<temp; i++){
        if(read(monsters[i].socket_val_gw, buf, sizeof(monster_response)+1) < 0) exit(-1);
        monster_response *received_m = (monster_response *) buf;
        if (received_m->mr_type==mr_dead){
            //cout << "Monster(" << monsters[i].position.x << "," << monsters[i].position.y << ") died and has responded back to game world. " << endl;
            world_map.monster_coordinates[i+deleted].x = 9999;
            world_map.monster_coordinates[i+deleted].y  = 9999; // block this from being printed
            waitpid(monsters[i].m_pid, &monster_status, WNOHANG); // no zombie wanted :)
            //if(WIFEXITED(monster_status)) cout << "monster " << monsters[i].position.x << "," << monsters[i].position.y << " has exitted by dying normally." << endl;
            //else cout << "monster " << monsters[i].position.x << "," << monsters[i].position.y << " has exitted abnormally." << endl;
            close(monsters[i].socket_val_gw); // close fd for that particular monster !
            monsters.erase(monsters.begin()+i); // shrink your alive monsters vector
            temp = (--world_map.alive_monster_count);
            deleted++;
            --i; // note that vector is shrinking along the way
            if(world_map.alive_monster_count==0) {go =go_survived; break;}

        }
        else if(received_m->mr_type==mr_move){
            //cout << "Monster(" << monsters[i].position.x << "," << monsters[i].position.y << ") has responded back to game world and wants to move. " << endl;
            int move_cond = process_move(1, NULL, received_m);
            if(move_cond){ // monster movement is accepted
                monsters[i].position.x=received_m->mr_content.move_to.x;
                monsters[i].position.y=received_m->mr_content.move_to.y; //update monsters vector
                //cout << "Successfully moved to " << received_m->mr_content.move_to.x << " " << received_m->mr_content.move_to.y << endl;
                world_map.monster_coordinates[i+deleted].x=received_m->mr_content.move_to.x;
                world_map.monster_coordinates[i+deleted].y=received_m->mr_content.move_to.y;
            }
        }
        else if(received_m->mr_type==mr_attack){
            //cout << "Monster(" << monsters[i].position.x << "," << monsters[i].position.y << ") has responded back to game world and wants to attack back with damage of " << received_m->mr_content.attack << endl;
            t_damage_on_player += received_m->mr_content.attack;
        }
    }
    sort_monsters_and_game_world_monsters(); // rearrange monster coordinates
    return 1;
}

int main(){
    initialize_inp();
    pid_game = getpid();
    int  fork_val;
    if ( PIPE(sockets)< 0) {
        perror("stream socket pair failed");
        exit(0);
    }

    if ((fork_val = fork() ) == -1)  {
        perror("fork failed");
        return 0;
    }

    if(fork_val==0){ // player is child it uses sockets[1]
        pid_player=getpid();
        close(sockets[0]);
        dup2(sockets[1],0);
        dup2(sockets[1],1);
        close(sockets[1]);
        string str = to_string( world_map.door.x);
        char *cstr1 = new char[str.length() + 1];
        strcpy(cstr1, str.c_str());
        str = to_string( world_map.door.y);
        char *cstr2 = new char[str.length() + 1];
        strcpy(cstr2, str.c_str());

        char *const argv[7] = {playerExecName, cstr1, cstr2, player_attack_count, player_distance, num_turns, NULL};
        if(execv(playerExecName, argv )) exit(-1);
    }
    else{ // game world uses sockets[0] for player and uses a m_sockets[0] for each monster
        close(sockets[1]);
        if(read(sockets[0], buf, sizeof(player_response)+1) < 0) exit(-1);
        player_response *received_p = (player_response *) buf;
        if (!(received_p->pr_type==pr_ready)) {gameover_messages(0); exit(-1);}

        for(int i=0; i<num_monsters; i++){
            int m_sockets[2];
            if ( PIPE(m_sockets)< 0) {
                perror("stream socket pair failed");
                gameover_messages(0);
                exit(0);
            }
            // CONSTRUCTION OF MONSTERS
             if(fork()==0){
                 monsters[i].m_pid=getpid();
                 close(sockets[0]); // game world's socket should not be copied to monster
                 close(m_sockets[0]); // monster uses stdio
                 dup2(m_sockets[1],0);
                 dup2(m_sockets[1],1);
                 close(m_sockets[1]);
                 char *const argv[7] = {monsters[i].execMonsterName,  monsters[i].health, monsters[i].damage_val, monsters[i].defence_val,  monsters[i].range, NULL};
                 if(execv(monsters[i].execMonsterName, argv ) ){gameover_messages(0); exit(-1);}
             }
             else{
                 close(m_sockets[1]); // game world's monster end is m_sockets[0]
                 monsters[i].socket_val_gw = m_sockets[0]; // save the game world fd value into monster
                 if(read(m_sockets[0], buf, sizeof(monster_response)+1) < 0) {gameover_messages(0); exit(-1);}
                 monster_response *received_m = (monster_response *) buf;
                 if (!(received_m->mr_type==mr_ready)) {gameover_messages(0); exit(-1);}
             }
        }

        sort_monsters_and_game_world_monsters();
        print_map(&world_map);

        // initial check for player's lucky position on door
        if(world_map.player.x==door_x && world_map.player.y==door_y) { go=go_reached; print_game_over(go); print_map(&world_map); gameover_messages(0); return 0;}
        if(world_map.alive_monster_count==0) {go =go_survived; print_game_over(go); print_map(&world_map); gameover_messages(0); return 0;}
        int i=0;

        while(1){
            // send player to her information
            if( send_player_message(world_map.player.x, world_map.player.y, world_map.alive_monster_count, t_damage_on_player, 0, world_map.monster_coordinates)!=0) { gameover_messages(0); exit(-3);}

            // has player rage quitted ?
            // receive response of player
            player_response* player_received = receive_player_response();
            if(player_received==NULL) { print_map(&world_map); go=go_left; gameover_messages(1);  break;}
            else if(player_received->pr_type==pr_move) {
                // cout << "player wants to move. " << endl;
                int move_st=process_move(0, player_received, NULL);
                //if(move_st==1) cout << "player moved to " << world_map.player.x << " and " << world_map.player.y << endl;
                if(move_st==2) { // player has reached the door
                     print_map(&world_map); gameover_messages(0); break;
                }
                for( i=0; i<world_map.alive_monster_count; i++){
                    send_monster_message(monsters[i].socket_val_gw, world_map.monster_coordinates[i].x, world_map.monster_coordinates[i].y, 0, world_map.player.x, world_map.player.y, 0);
                }
            }
            else if(player_received->pr_type==pr_attack){
                //cout << "player wants to attack. " << endl;
                for( i=0; i<world_map.alive_monster_count; i++){
                    int new_damage = player_received->pr_content.attacked[i];
                    //if(monsters[i].position.x!=9999 && monsters[i].position.y!=9999) cout << "Player is attacking Monster(" << monsters[i].position.x << "," << monsters[i].position.y << ") with damage of " << new_damage << endl;
                    send_monster_message(monsters[i].socket_val_gw, world_map.monster_coordinates[i].x, world_map.monster_coordinates[i].y, new_damage, world_map.player.x, world_map.player.y, 0);
                }
             }
            else if(player_received->pr_type==pr_dead){
                //cout << "player says she is dead. " << endl;
                print_map(&world_map); go=go_died; gameover_messages(1); break;
            }

            if(!receive_monster_responses()) {gameover_messages(0); exit(-2); } // collect answers from monsters and process them.

            print_map(&world_map);

            if(go==go_survived){ // no monster left
               gameover_messages(0); break;
            }
            //cout << "Monster responses are collected, sending new message to player with total monster attack of " << t_damage_on_player << endl;
        }

        print_game_over(go);

    }
    return 0;
}
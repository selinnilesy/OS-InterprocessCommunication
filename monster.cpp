//
// Created by Selin Yıldırım on 8.04.2021.
//
#include "logging.c"
#include<iostream>
#include <unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include <signal.h>
#include<sys/wait.h>
using namespace std;
char buf[1024], buf2[1024];

typedef struct monster {
    coordinate position;
    int health;
    char name; /// remember this is not initialized
    int range;
    int damage_val;
    int defence_val;
} monster;

monster me;
int player_x, player_y;
bool game_over=0;

void ready_response(){
    monster_response a;
    a.mr_type = mr_ready;
    a.mr_content.move_to.x=a.mr_content.move_to.y=0;
    char *byteStream = (char *) &a;
    if (write(1, byteStream, sizeof(monster_response)) < 0) perror("error when writing response");
}
void send_response(bool dead, bool attack, bool move, int move_x, int move_y){
    monster_response a;
    a.mr_content.attack=0; // default settings
    a.mr_content.move_to.x=move_x, a.mr_content.move_to.y=move_y;

    if(dead) a.mr_type = mr_dead;
    else if(attack){
        a.mr_type = mr_attack;
        a.mr_content.attack=me.damage_val;
    }
    else if(move){
        a.mr_type = mr_move;
    }
    char *byteStream = (char *) &a;
    if (write(1, byteStream, sizeof(monster_response)) < 0) perror("error when writing dead response ");
}
int get_game_message(){
    if(read(0, buf, sizeof(monster_message)+1) < 0) return -1;
    monster_message *received_p = (monster_message *) buf;
    game_over=received_p->game_over;
    if(game_over) return 0;
    me.position.x = received_p->new_position.x, me.position.y=received_p->new_position.y;
    player_x = received_p->player_coordinate.x , player_y = received_p->player_coordinate.y;
    me.health = me.health - max(0, received_p->damage - me.defence_val); // generic message implementation, monster does not know if she is under attack or not
    return 1;
}
int manhattan_distance(int player_x, int player_y, int me_x, int me_y){
    int distance = abs(player_x-me_x) + abs(player_y-me_y);
    return distance;
}
int main(int argc, char** argv){
    me.health=stoi(argv[1]), me.damage_val=stoi(argv[2]),me.defence_val=stoi(argv[3]),me.range=stoi(argv[4]);
    ready_response();
    while(1) {
        int res = get_game_message();
        if (res == -1) {
            //cout << "monster cannot read the message of game world. what a pity!" << endl;
            break;
        }
        if (me.health <= 0 || res == 0) { // game over or health is over
            send_response(1, 0, 0, 0, 0); break;
        }
        else {
            int distance = manhattan_distance(player_x, player_y, me.position.x, me.position.y);
            if (distance <= me.range) { // monster decides to attack
                send_response(0, 1, 0, 0, 0);
            }
            else { // monster needs to move
                coordinate possibility[8];
                possibility[0].x = me.position.x, possibility[0].y = me.position.y - 1;
                possibility[1].x = me.position.x + 1, possibility[1].y = me.position.y - 1;
                possibility[2].x = me.position.x + 1, possibility[2].y = me.position.y;
                possibility[3].x = me.position.x + 1, possibility[3].y = me.position.y + 1;
                possibility[4].x = me.position.x, possibility[4].y = me.position.y + 1;
                possibility[5].x = me.position.x - 1, possibility[5].y = me.position.y + 1;
                possibility[6].x = me.position.x - 1, possibility[6].y = me.position.y;
                possibility[7].x = me.position.x - 1, possibility[7].y = me.position.y - 1;
                int res_index = -1, min_dist = 9999;
                for (int i = 0; i < 8; i++) {
                    int temp_d = manhattan_distance(player_x, player_y, possibility[i].x, possibility[i].y);
                    if (temp_d < min_dist) {
                        min_dist = temp_d;
                        res_index = i;
                    }
                }
                send_response(0, 0, 1, possibility[res_index].x, possibility[res_index].y);
            }
        }
    }

    close(0);
    close(1);
    exit(0);
}
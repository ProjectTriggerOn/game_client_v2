///ゲーム本体///

#ifndef GAME_H
#define GAME_H

void Game_Initialize();

void Game_Update(double elapsed_time);

void Game_Draw();

void Game_Finalize();

void CollisionDetect_Bullet_Enemy();

void CollisionDetect_Player_Enemy();



#endif 
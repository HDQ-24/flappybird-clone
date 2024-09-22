#include <SDL.h> 
#include <SDL_image.h> 
#include <SDL_mixer.h> 
#include <stdio.h>
#include <stdlib.h> 
#include <math.h> 
#include <time.h> 
#include <vector> 
#define GET_RANDOM(min,max) ((rand() % (max-min+1))+min)  


/************************************************************************/ 
 
const int SCREEN_WIDTH  = 288;
const int SCREEN_HEIGHT = 512;  

SDL_Renderer* window_ren;
SDL_Window* my_window;

// Flags 
bool jumping    = false;  
bool dead       = false; 
bool top_scored = false; 
bool start      = false; 
bool retry      = false; 


// Animation variables 
int bird_clip_i   = 0;
int bird_clip_d   = 5;       // in frames 
int frame_counter = 0; 


// Main game variables
int k = 0; 
float fall_speed   = 0.2;
double bird_angle  = 0.0;
double angle_speed = 3.0;     // + every frame 
int floor_speed    = 2;       // factor of floor width 
int score          = 0; 
int pipe_timer     = 0; 
 

// DYING fading out variables 
bool fade_out     = false;
int fade_duration = 16; // in frames 
int fade_speed    = 255/8;
int fade_count    = 0; 


// RETRYING fading out variables 
bool fade_out2     = false;
int fade_duration2 = 32; // in frames 
int fade_speed2    = 255/16;
int fade_count2    = 0; 
int alpha = 255;         // default alpha used by all sprites 


// SPRITES fading out variables 
int gameover_alpha   = 0;
int title_alpha      = 255; 			
bool gameover_fading = false; 
bool title_fading    = false; 


// Dinamic lists used 
std::vector <std::vector <SDL_Rect>> pipe_q;  // 0: upper pipe, 1: lower pipe 
std::vector <int> score_digits = {0};  



/************************************************************************/ 

class Sprite {
private: 
	SDL_Texture* texture;
public:
	int w, h;	
	Sprite(const char* path);
	void Render(SDL_Rect* src, SDL_Rect* dest, double angle = 0.0, int alpha = 255);
}; 

Sprite::Sprite(const char* path) 
{
	SDL_Surface* temp = IMG_Load(path);
	w = temp->w;
	h = temp->h; 
	texture = SDL_CreateTextureFromSurface(window_ren, temp);
	SDL_FreeSurface(temp);  
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);     // allow texture blending and transparency 
}
void Sprite::Render(SDL_Rect* src, SDL_Rect* dest, double angle, int alpha) 
{ 
	SDL_SetTextureAlphaMod(texture, alpha);                   // allows fading out effect 
	SDL_RenderCopyEx(window_ren, texture, src, dest, angle, NULL, SDL_FLIP_NONE); 
}



/************************************************************************/ 
 
class Transitioner {
private: 
	int* sprite_alpha;    // associates with a sprite 
	SDL_Rect* sprite_pos; 	
	
	int move_total, move_speed, move_i;  // internal variables 
	bool move_x, move_y; 
	int fading_total, fading_speed, fading_i;
	
public: 	
	Transitioner(SDL_Rect* sprite_pos, int* sprite_alpha);
	void SetTransition(int n, int speed, bool move_x, bool move_y); 
	void SetFading(int n, int speed);
	int TransitionFoward(); 
	int FadingFoward(); 
};
Transitioner::Transitioner(SDL_Rect* sprite_pos, int* sprite_alpha)  // pointers to its fields
{
	move_total = 0;
	move_speed = 0;
	move_i = 0;
	
	move_x = false;
	move_y = false;
	
	fading_total = 0;
	fading_speed = 0;
	fading_i = 0; 
	
	this->sprite_alpha = sprite_alpha; 
	this->sprite_pos = sprite_pos; 
}
void Transitioner::SetTransition(int n, int speed, bool move_x, bool move_y)
{ 	
	if(move_i == 0) {  // not in a middle of a transition 
		move_total = n;
		move_speed = speed; 
		this->move_x = move_x; 
		this->move_y = move_y; 		
	}
}
int Transitioner::TransitionFoward()
{
	if(move_i < move_total) {
		if(move_y)
			sprite_pos->y += move_speed; 
		if(move_x)
			sprite_pos->x += move_speed; 
		move_i++;
	} else {
		move_i = 0; 
		return 0;     // if finish, return 0, re-start
	}  
	return 1;         // if not finish, return 1 
}
void Transitioner::SetFading(int n, int speed)
{
	if(fading_i == 0) {
		fading_total = n;
		fading_speed = speed; 
		(*sprite_alpha) = (fading_speed < 0) ? 255 : 0;  // disappear or not 
	}
}
int Transitioner::FadingFoward()
{
	if(fading_i < fading_total) {
		(*sprite_alpha) += fading_speed; 
		fading_i++; 
	} else { 
		fading_i = 0;
		(*sprite_alpha) = (fading_speed < 0) ? 0 : 255;  // a litle correction ...
		return 0; 
	}
	return 1; 
}



/************************************************************************/ 
	
void Reset()         // Reset some globals to their default values 
{
	k = 0; 
	fall_speed  = 0.2;
	dead        = false; 
	top_scored  = false; 
	start       = false; 
	bird_angle  = 0.0;
	score       = 0; 
	pipe_timer  = 0; 
	floor_speed = 2; 
	title_alpha = 255;
	gameover_alpha = 0; 
	
	while(!pipe_q.empty())  // destroying everything 
		pipe_q.pop_back();
		
	while(!score_digits.empty()) 
		score_digits.pop_back();
	score_digits.push_back(0); 
}

bool RectCollision(SDL_Rect r1, SDL_Rect r2) 
{
	int x_start, x_end; 
	int y_start, y_end; 
	
	if(r2.y < r1.y)    // if r2 is above r1 
		y_start = r2.y, y_end = r1.y+r1.h;  
	else               // if r1 is above r2 
		y_start = r1.y, y_end = r2.y+r2.h; 
	
	if(r2.x < r1.x)    // if r2 is left to r1 
		x_start = r2.x, x_end = r1.x+r1.w;  
	else               // if r1 is left to r2 
		x_start = r1.x, x_end = r2.x+r2.w; 
	
	return ((y_end-y_start) < (r1.h+r2.h)) && ((x_end-x_start) < (r1.w+r2.w));  // vertical collision and horizontal collision 
}


int main(int argc, char** argv) 
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);	 
	IMG_Init(IMG_INIT_PNG);
	 
	my_window  = SDL_CreateWindow("Flappy Bird", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN); 
	window_ren = SDL_CreateRenderer(my_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_SetRenderDrawColor(window_ren, 0xFF, 0xFF, 0xFF,0xFF);
	
	// Loading sprites 
	Sprite background_sprite = Sprite("assets/sprites/background-day.png");	
	Sprite floor_sprite      = Sprite("assets/sprites/base.png");  
	Sprite gameover_sprite   = Sprite("assets/sprites/gameover.png");
	Sprite title_sprite      = Sprite("assets/sprites/message.png");
	Sprite pipe_sprite       = Sprite("assets/sprites/pipe-green.png");
	
	Sprite bird_sprites []   = {{"assets/sprites/yellowbird-downflap.png"}, 
								{"assets/sprites/yellowbird-midflap.png"}, 
								{"assets/sprites/yellowbird-upflap.png"}}; 
	Sprite digit_sprites[]   = {{"assets/sprites/0.png"}, {"assets/sprites/1.png"}, {"assets/sprites/2.png"}, 
							    {"assets/sprites/3.png"}, {"assets/sprites/4.png"}, {"assets/sprites/5.png"},
					            {"assets/sprites/6.png"}, {"assets/sprites/7.png"}, {"assets/sprites/8.png"},
					            {"assets/sprites/9.png"}}; 
	
	// Dest Rectangles 
	SDL_Rect floor1   = {0, SCREEN_HEIGHT - floor_sprite.h, floor_sprite.w, floor_sprite.h};             // onscreen 
	SDL_Rect floor2   = {floor_sprite.w, SCREEN_HEIGHT - floor_sprite.h, floor_sprite.w, floor_sprite.h};       // offscreen 
	SDL_Rect bird     = {1*SCREEN_WIDTH/6, (SCREEN_HEIGHT - bird_sprites[0].h)/2, bird_sprites[0].w, bird_sprites[0].h}; 	
	SDL_Rect title    = {(SCREEN_WIDTH - title_sprite.w)/2, 1*SCREEN_HEIGHT/8, title_sprite.w, title_sprite.h};   // (x,y) is an aproximation 
	SDL_Rect gameover = {(SCREEN_WIDTH - gameover_sprite.w)/2, 1*SCREEN_HEIGHT/8, gameover_sprite.w, gameover_sprite.h};
	
	// All Transitioners 
	Transitioner bird_transitioner     = Transitioner(&bird, NULL); 
	Transitioner title_transitioner    = Transitioner(&title, &title_alpha);
	Transitioner gameover_transitioner = Transitioner(&gameover, &gameover_alpha);
	
	bird_transitioner    .SetTransition(6, -7, false,true);                            // setup jump 
	gameover_transitioner.SetTransition(20, 2, false, true); 
	title_transitioner   .SetFading(20, -255/20);
	gameover_transitioner.SetFading(15,  255/15);              // setup for the game over 
	
	
	// Loading Sound 
	Mix_Chunk* jump_sound   = Mix_LoadWAV("assets/audio/wing.wav"); 
	Mix_Chunk* dead_sound   = Mix_LoadWAV("assets/audio/die.wav"); 
	Mix_Chunk* hit_sound    = Mix_LoadWAV("assets/audio/hit.wav"); 
	Mix_Chunk* point_sound  = Mix_LoadWAV("assets/audio/point.wav"); 
	Mix_Chunk* swoosh_sound = Mix_LoadWAV("assets/audio/swoosh.wav"); 
	
	// Game Loop 
	SDL_Event e; 
	bool quit = false; 
	while(!quit) {
		while(SDL_PollEvent(&e) != 0) {
			if(e.type == SDL_QUIT)
				quit = true; 
			
			if(!start && e.type == SDL_MOUSEBUTTONDOWN) {
				start        = true; 
				title_fading = true; 
			} 
			if(!dead && e.type == SDL_MOUSEBUTTONDOWN) {  // if not dead, jump 
				jumping    = true;
				bird_angle = -35.0;                       // facing up 
				Mix_PlayChannel(-1, jump_sound, 0); 
			}
			if(retry && e.type == SDL_MOUSEBUTTONDOWN) {   // if you are able to retry, activates the black fade out code (that eventually calls restart) 
				fade_out2 = true; 
				retry     = false; 
				Mix_PlayChannel(-1, swoosh_sound, 0);
				SDL_SetRenderDrawColor(window_ren, 0x00, 0x00, 0x00,0x00);  
			}
		}
		
		// Rendering 
		SDL_RenderClear(window_ren);
		background_sprite.Render(NULL, NULL, 0.0, alpha);
		
		for(int i = 0; i < pipe_q.size(); i++) { // render all pipes 
			pipe_sprite.Render(NULL, &pipe_q[i][0], 180.0, alpha);
			pipe_sprite.Render(NULL, &pipe_q[i][1], 0.0  , alpha);
		}
		
		if(start) {     // if the game doesnt start, dont render the score 
			if(dead) {  // once dead it would render the game over instead of the score ,  i may remove this and just show the score ... 
				if(fade_out2) 
					gameover_sprite.Render(NULL, &gameover, 0.0, alpha);           
				else 
					gameover_sprite.Render(NULL, &gameover, 0.0, gameover_alpha);
				 
				if(gameover_fading) {
					gameover_fading = gameover_transitioner.FadingFoward(); 
					gameover_transitioner.TransitionFoward();  // they would go at the same time so we only save one flag  ... 
					if(!gameover_fading)                       // after showing the game over, you can click (retry)
						retry = true; 
				}				
			} else {   
				int score_base_x = (SCREEN_WIDTH - score_digits.size()*digit_sprites[0].w)/2;  // Render the current score  
				for(int i = score_digits.size()-1, k = 0; i >= 0; i--, k++) {           // renders the current score (centered)
					SDL_Rect digit = {score_base_x + digit_sprites[0].w*k, SCREEN_HEIGHT/6, digit_sprites[0].w, digit_sprites[1].h};
					digit_sprites[score_digits[i]].Render(NULL, &digit, 0.0, alpha);
				}
			}	
		}
		
		bird_sprites[bird_clip_i].Render(NULL, &bird, bird_angle, alpha);
		floor_sprite.Render(NULL, &floor1, 0.0, alpha);
		floor_sprite.Render(NULL, &floor2, 0.0, alpha); 
		title_sprite.Render(NULL, &title , 0.0, title_alpha);     // render the game start screen, it would be un-visible after the fading... 
		
		SDL_RenderPresent(window_ren);
		
		if(title_fading)  
			title_fading = title_transitioner.FadingFoward(); 
		
		// Floor logic 
		floor1.x -= floor_speed;  
		floor2.x -= floor_speed;
		if((floor1.x + floor_sprite.w) <= 0)
			floor1.x = floor_sprite.w;  
		if((floor2.x + floor_sprite.w) <= 0)
			floor2.x = floor_sprite.w; 
		
		// moving the visible pipes if any 
		for(int i = 0; i < pipe_q.size(); i++) { 
			pipe_q[i][0].x -= floor_speed;
			pipe_q[i][1].x -= floor_speed;
		}
		 
		// checking for collisions 
		for(int i = 0; i < pipe_q.size(); i++) { 
			if(!dead && (RectCollision(bird, pipe_q[i][0]) || RectCollision(bird, pipe_q[i][1]))) {  // checking for upper/lower pipe 
				dead = true; 
				fade_out = true; 
				floor_speed = 0; 
				Mix_PlayChannel(-1, hit_sound, 0);
				Mix_PlayChannel(-1, dead_sound, 0);
				break; 
			}
		}
		if(RectCollision(bird, floor1) || RectCollision(bird, floor2)) { 
			bird.y = floor1.y - bird.h;                // stays in the ground 
			fall_speed = 0; 
			k = 0;
			if(!dead) {
				dead = true; 
				fade_out = true; 
				floor_speed = 0;
				Mix_PlayChannel(-1, hit_sound, 0);
			}
		}
		
		// checking if certain objects are offscreen 
		for(int i = 0; i < pipe_q.size(); i++) { 
			if(pipe_q[i][0].x + pipe_sprite.w <= 0)  {
				pipe_q.erase(pipe_q.begin());
				top_scored = false;               // after the deleting the prev pipe, the new top is tecnically not scored 
			}
		}
		
		// Only if the game started, execute this 
		if(start) {   
			// Falling and jump logic 
			if(jumping) {                          
				jumping = bird_transitioner.TransitionFoward();
				if(!jumping) 
					k = 0;                          // reset the factor so it starts falling slow again 				 
			} else {                                // if not jumping, starts falling  
				bird.y += round(fall_speed*k);      // k = 'acceleration' 
				k++;
			}
			// Bird angle logic 
			if(bird_angle < 90.0 && !jumping)  
				bird_angle += angle_speed; 
			
			// pipe generator 
			if((SDL_GetTicks() - pipe_timer) >= 1500) {
				int gap_levels[] = {(SCREEN_HEIGHT-300)/2, (SCREEN_HEIGHT-300)/2 + 100, (SCREEN_HEIGHT-300)/2 + 150}; // gap size : 100, 3 different 'y' positions 
				int n = GET_RANDOM(0, 2);
				 
				SDL_Rect upper_pipe = {SCREEN_WIDTH, gap_levels[n] - pipe_sprite.h, pipe_sprite.w, pipe_sprite.h};
				SDL_Rect lower_pipe = {SCREEN_WIDTH, gap_levels[n] + 100          , pipe_sprite.w, pipe_sprite.h}; 
				pipe_q.push_back({upper_pipe, lower_pipe});
				pipe_timer = SDL_GetTicks();
			}
		}				
		
		// Fading effect after DYING
		if(fade_out) {  // starts a counter 
			if(fade_count < fade_duration/2) {  // on first 10 frames 
				alpha -= fade_speed; 
				fade_count++; 
			} else if(fade_count < fade_duration){  // on second 10 frames 
				alpha += fade_speed;
				fade_count++; 
			} else {
				fade_out = false;
				fade_count = 0;
				gameover_fading = true;                                 
			}				
		}
		// Fading effect after RETRYING (clicking) 			
		if(fade_out2) {  
			if(fade_count2 < fade_duration2/2) {  // on first 10 frames 
				alpha -= fade_speed2; 
				fade_count2++; 
			} else if(fade_count2 < fade_duration2){  // on second 10 frames 

				if(fade_count2 == fade_duration2/2) { // after first black fade out, we reset 
					Reset(); 
					bird.y = (SCREEN_HEIGHT - bird_sprites[0].h)/2;    // reposition the bird and gameover 
					gameover.y = SCREEN_HEIGHT/8; 					
				}
				alpha += fade_speed2;
				fade_count2++; 
			} else {
				fade_out2 = false;
				fade_count2 = 0;
				SDL_SetRenderDrawColor(window_ren, 0xFF, 0xFF, 0xFF,0xFF);   // go white again after both fade outs 
			}				
		}
		
		// scoring logic 
		if(!pipe_q.empty()) {			
			if(!top_scored && ((bird.x + bird.w) >= (pipe_q[0][0].x + pipe_q[0][0].w))) {   // if we didnt score the top
				score++;
				Mix_PlayChannel(-1, point_sound, 0); 
				 
				while(!score_digits.empty())   // removing the prev score 
					score_digits.pop_back();
				
				int score_cpy = score;  // extract the digits and storing the new score 
				while(score_cpy != 0) {
					score_digits.push_back(score_cpy % 10);
					score_cpy /= 10; 
				}
				top_scored = true;                                                                
			}
		}
		
		// Bird animation 
		if(!dead) {  // if dead, stop the animation 
			frame_counter++; 
			if(frame_counter % bird_clip_d == 0) {
				bird_clip_i++; 
				frame_counter = 0; 
			}
			if(bird_clip_i % 3 == 0) 
				bird_clip_i = 0; 					
		}
	}
	// Closing the game 
	SDL_DestroyWindow(my_window);                
	SDL_DestroyRenderer(window_ren);

	Mix_FreeChunk(dead_sound);
	Mix_FreeChunk(swoosh_sound);
	Mix_FreeChunk(jump_sound);
	Mix_FreeChunk(hit_sound);
	Mix_FreeChunk(point_sound);
	 
	my_window    = NULL; 
	window_ren   = NULL; 
	dead_sound   = NULL;
	swoosh_sound = NULL;
	jump_sound   = NULL;
	hit_sound    = NULL;
	point_sound  = NULL; 
	
	Mix_Quit();
	SDL_Quit();	
	return 0; 
}	




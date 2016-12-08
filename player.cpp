#include "player.h"

#include "map.h"
#include "entity.h"
#include "assets.h"

// FIXME Cannot fall in a single tile

#define ATTACK_TOTAL_DURATION 16
#define ATTACK_CHARGE 10

#define SPRITE_ORIGIN_X 8
#define SPRITE_ORIGIN_Y 16

#define FRAME_IDLE 0
#define FRAME_WALK_1 0
#define FRAME_WALK_2 2
#define FRAME_ATTACK_CHARGE 4
#define FRAME_ATTACK 6
#define FRAME_AIR 8
#define FRAME_KNOCKBACK 9
#define FRAME_DEAD 10

#define FRAME_FLIPPED_OFFSET 11

#define WALK_FRAME_RATE 12

uint8_t Player::hp;
Vec Player::pos;
bool Player::alive;

namespace
{
const Box normalHitbox =
{
  4, 14, // x, y
  8, 14  // width, height
};

const Box duckHitbox =
{
  4, 6, // x, y
  8, 6  // width, height
};

int8_t velocityX;
int16_t velocityYf;
// FIXME move flags to a single byte and use masking (is it worth it?)
bool grounded;
uint8_t attackCounter;
bool attacking;
bool jumping;
bool ducking;
uint8_t knockbackCounter;
uint8_t levitateCounter;
bool flipped;
bool walkFrame;

} // unamed

void Player::init(int16_t x, int8_t y)
{
  pos.x = x;
  pos.y = y;
  grounded = false;
  attackCounter = 0;
  alive = true;
  attacking = false;
  jumping = false;
  ducking = false;
  knockbackCounter = 0;
  levitateCounter = 0;
  velocityX = 0;
  velocityYf = 0;
}

void Player::update()
{
  // knockback
  if (knockbackCounter > 0)
  {
    if (--knockbackCounter == 0)
    {
      velocityX = 0;
      if (hp == 0)
      {
        alive = false;
      }
    }
  }

  if (!alive)
  {
    return;
  }

  // attack
  if (knockbackCounter == 0 && attackCounter == 0 && ab.justPressed(B_BUTTON))
  {
    attackCounter = ATTACK_TOTAL_DURATION;
    sound.tone(NOTE_GS4, 10);
  }

  if (attackCounter > 0)
  {
    --attackCounter;
  }

  // jump
  if (knockbackCounter == 0 && !ducking && attackCounter == 0 && grounded && ab.justPressed(A_BUTTON))
  {
    // start jumping
    grounded = false;
    jumping = true;
    velocityYf = -PLAYER_JUMP_FORCE_F;
  }

  // vertical movement
  if (levitateCounter > 0)
  {
    --levitateCounter;
  }
  else if (jumping)
  {
    velocityYf += PLAYER_JUMP_GRAVITY_F;
    if (velocityYf >= 0)
    {
      velocityYf = 0;
      jumping = false;
      levitateCounter = PLAYER_LEVITATE_DURATION;
    }
    else
    {
      Map::moveY(pos, velocityYf / F_PRECISION, ducking ? duckHitbox : normalHitbox);
    }
  }
  else
  {
    velocityYf += PLAYER_FALL_GRAVITY_F;
    int16_t offsetY = velocityYf / F_PRECISION;
    if (offsetY > 0)
    {
      grounded = Map::moveY(pos, offsetY, ducking ? duckHitbox : normalHitbox);
    }
    else
    {
      grounded = Map::collide(pos.x, pos.y + 1, ducking ? duckHitbox : normalHitbox);
    }

    if (grounded)
    {
      velocityYf = 0;
    }
  }

  // horizontal movement
  if (knockbackCounter == 0)
  {
    if (attackCounter == 0 && ab.pressed(LEFT_BUTTON))
    {
      velocityX = -1;
      //verticalMoveCounter = 0;
      flipped = true;
    }
    else if (attackCounter == 0 && ab.pressed(RIGHT_BUTTON))
    {
      velocityX = 1;
      // verticalMoveCounter = 0;
      flipped = false;
    }
    else if (grounded)
    {
      velocityX = 0;
    }
  }

  if (velocityX != 0 &&
      (
        knockbackCounter == 0 && ab.everyXFrames(ducking ? PLAYER_SPEED_DUCK : PLAYER_SPEED_NORMAL) ||
        knockbackCounter > 0 && ab.everyXFrames(knockbackCounter < PLAYER_KNOCKBACK_FAST ? PLAYER_SPEED_KNOCKBACK_NORMAL : PLAYER_SPEED_KNOCKBACK_FAST)
      )
     )
  {
    if (!Map::collide(pos.x + velocityX, pos.y, ducking ? duckHitbox : normalHitbox))
    {
      pos.x += velocityX;
    }
  }

  // duck
  if (knockbackCounter == 0 && attackCounter == 0)
  {
    if (!ducking)
    {
      ducking = grounded && ab.pressed(DOWN_BUTTON);
    }
    else if (!ab.pressed(DOWN_BUTTON))
    {
      // only stop ducking if player can stand
      ducking = Map::collide(pos.x, pos.y, normalHitbox);
    }
  }

  // perform attack
  if (attackCounter != 0 && attackCounter < ATTACK_CHARGE)
  {
    Entities::attack(pos.x + (flipped ? -24 : 0), pos.y - (ducking ? 3 : 11), pos.x + (flipped ? 0 : 24));
  }

  // check if player falled in a hole
  if (pos.y - SPRITE_ORIGIN_Y > 64)
  {
    alive = false;
  }

  // check entity collision
  if (knockbackCounter == 0)
  {
    Entity* entity = Entities::collide(pos, ducking ? duckHitbox : normalHitbox);
    if (entity != NULL)
    {
      flipped = entity->pos.x < pos.x;
      velocityX = flipped ? 1 : -1;
      knockbackCounter = PLAYER_KNOCKBACK_DURATION;
      jumping = false;
      levitateCounter = 0;
      attackCounter = 0;
      if(--hp == 0)
      {
        alive = false;
      }
      flashCounter = 2;
      sound.tone(NOTE_GS3, 25, NOTE_G3, 15);
    }
  }
}

void Player::draw()
{
  uint8_t frame = 0;

  if (!alive)
  {
    frame = FRAME_DEAD;
  }
  else if (knockbackCounter > 0)
  {
    frame = FRAME_KNOCKBACK;
  }
  else if (attackCounter == 0 && !grounded)
  {
    frame = FRAME_AIR;
  }
  else
  {
    if (attackCounter == 0)
    {
      if (velocityX == 0)
      {
        frame = FRAME_IDLE;
      }
      else
      {
        if (ab.everyXFrames(WALK_FRAME_RATE))
        {
          walkFrame = !walkFrame;
        }
        frame = walkFrame ? FRAME_WALK_2 : FRAME_WALK_1;
      }
    }
    else if (attackCounter < ATTACK_CHARGE)
    {
      frame = FRAME_ATTACK;
    }
    else
    {
      frame = FRAME_ATTACK_CHARGE;
    }

    if (ducking)
    {
      frame++;
    }
  }

  sprites.drawPlusMask(pos.x - SPRITE_ORIGIN_X - Game::cameraX, pos.y - SPRITE_ORIGIN_Y, player_plus_mask, frame + (flipped ? FRAME_FLIPPED_OFFSET : 0));

  if (attackCounter != 0 && attackCounter < ATTACK_CHARGE)
  {
    sprites.drawPlusMask(pos.x + (flipped ? -24 : 8) - Game::cameraX , pos.y - (ducking ? 4 : 12), flipped ? player_attack_left_plus_mask : player_attack_right_plus_mask, 0);
  }

#ifdef DEBUG_HITBOX
  Rect hitbox = ducking ? duckHitbox : normalHitbox;
  ab.fillRect(pos.x - hitbox.x - Game::cameraX, pos.y - hitbox.y, hitbox.width, hitbox.height);
#endif

}


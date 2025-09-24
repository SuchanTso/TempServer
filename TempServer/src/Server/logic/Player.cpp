#include"Spch.h"
#include"Player.h"

namespace Tso {
std::string Player::GetPlayerIDstr(){
    return std::to_string(m_ID);
}

}

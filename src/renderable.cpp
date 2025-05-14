#include "vk_types.hpp"

void Node::refreshTransform(const glm::mat4& parentMatrix)
{
    globalTransform = parentMatrix * localTransform;
    for (auto c : children) {
        c->refreshTransform(globalTransform);
    }
}

void Node::draw(const glm::mat4& topMatrix, DrawContext& ctx) 
{
    // draw children
    for (auto& c : children) {
        c->draw(topMatrix, ctx);
    }
}
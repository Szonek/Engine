#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // cpp
#include <stdint.h>

#define ENGINE_COMPOUND_COLLIDER_MAX_CHILD_COLLIDERS 16

/**
* @enum _engine_collider_type_t
* @brief An enumeration representing the types of colliders that can be used in the engine.
*
* This enumeration is used to specify the type of a collider component. The type determines
* which shape in the collider union of the `engine_collider_component_t` struct is used.
*
* @var ENGINE_COLLIDER_TYPE_NONE
* Represents the absence of a collider. When this type is set, no collision detection or
* response will be performed for the entity this collider is attached to.
*
* @var ENGINE_COLLIDER_TYPE_BOX
* Represents a box-shaped collider. When this type is set, the `box` field of the collider
* union in the `engine_collider_component_t` struct should be used. The `engine_box_collider_t`
* struct contains the dimensions of the box along the x, y, and z axes.
*
* @var ENGINE_COLLIDER_TYPE_SPHERE
* Represents a sphere-shaped collider. When this type is set, the `sphere` field of the collider
* union in the `engine_collider_component_t` struct should be used. The `engine_sphere_collider_t`
* struct contains the radius of the sphere.
*
* @var ENGINE_COLLIDER_TYPE_COMPOUND
* Represents a compound collider, which is made up of multiple simpler shapes. When this type is set,
* the `compound` field of the collider union in the `engine_collider_component_t` struct should be used.
* The `engine_compound_collider_t` struct contains a list of child colliders, each with its own type,
* shape, and transform.
*/
typedef enum _engine_collider_type_t
{
    ENGINE_COLLIDER_TYPE_NONE = 0,
    ENGINE_COLLIDER_TYPE_BOX = 1,
    ENGINE_COLLIDER_TYPE_SPHERE,
    ENGINE_COLLIDER_TYPE_COMPOUND,
} engine_collider_type_t;

/**
 * @struct _engine_box_collider_t
 * @brief A struct representing a box collider.
 *
 * This struct contains the dimensions of the box along the x, y, and z axes.
 */
typedef struct _engine_box_collider_t
{
    float size[3];
} engine_box_collider_t;

/**
 * @struct _engine_sphere_collider_t
 * @brief A struct representing a sphere collider.
 *
 * This struct contains the radius of the sphere.
 */
typedef struct _engine_sphere_collider_t
{
    float radius;
} engine_sphere_collider_t;

/**
 * @struct _engine_child_collider_t
 * @brief A struct representing a child collider in a compound collider.
 *
 * This struct contains a collider type, a transform, and a union of possible
 * collider shapes. The collider type determines which shape in the union is used.
 */
typedef struct _engine_child_collider_t
{
    engine_collider_type_t type;
    float transform[3];  // x,y,z position transform
    union
    {
        engine_box_collider_t box;
        engine_sphere_collider_t sphere;
    } collider;
} engine_child_collider_t;

/**
 * @struct _engine_compound_collider_t
 * @brief A struct representing a compound collider.
 *
 * This struct contains a list of child colliders. Each child collider is a
 * separate shape that makes up part of the compound shape.
 */
typedef struct _engine_compound_collider_t
{
    engine_child_collider_t children[ENGINE_COMPOUND_COLLIDER_MAX_CHILD_COLLIDERS];
} engine_compound_collider_t;

/**
 * @struct _engine_collider_component_t
 * @brief A struct representing a collider component in the physics engine.
 *
 * This struct is used to define the physical boundaries of an object for the purpose of 
 * detecting collisions with other objects. It contains a collider type, a flag indicating 
 * whether the collider is a trigger, a union of possible collider shapes, and properties 
 * for bounciness and static friction.
 *
 * @var type
 * The type of the collider. This is an enum value of type `engine_collider_type_t` which 
 * determines which shape in the collider union is used. It can be a box, sphere, or a 
 * compound collider.
 *
 * @var is_trigger
 * A boolean flag indicating whether the collider is a trigger. A trigger does not participate 
 * in collision response but it still detects collisions. When a collision is detected with 
 * a trigger, an event is fired but the physics engine does not automatically apply forces 
 * or change the motion of the objects involved.
 *
 * @var collider
 * A union of possible collider shapes. Depending on the `type` field, it can be a box, 
 * sphere, or a compound collider. The `engine_box_collider_t` struct contains the dimensions 
 * of the box along the x, y, and z axes. The `engine_sphere_collider_t` struct contains the 
 * radius of the sphere. The `engine_compound_collider_t` struct contains a list of child 
 * colliders, each with its own type, shape, and transform.
 *
 * @var bounciness
 * A float value representing the bounciness of the collider. This determines how much the 
 * object will bounce back after a collision. A value of 0 means no bounce, and a value of 1 
 * means a perfect bounce with no loss of kinetic energy.
 *
 * @var friction_static
 * A float value representing the static friction of the collider. Static friction is the 
 * friction that keeps an object at rest. It needs to be overcome to start moving the object.
 */
typedef struct _engine_collider_component_t
{
    engine_collider_type_t type;
    bool is_trigger;
    union 
    {
        engine_box_collider_t box;
        engine_sphere_collider_t sphere;
        engine_compound_collider_t compound;
    } collider;

    float bounciness;
    float friction_static;
} engine_collider_component_t;

#ifdef __cplusplus
}
#endif // cpp
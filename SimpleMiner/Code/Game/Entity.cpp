#include "Game/Entity.hpp"
#include "Game/World.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <vector>

Entity::Entity(World* world)
	: m_world(world)
{

}

void Entity::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);
}

void Entity::UpdatePhysics(float fixedDeltaSeconds)
{
	switch (m_physicsMode)
	{
	case PhysicsMode::WALKING:
		UpdatePhysicsWalking(fixedDeltaSeconds);
		break;
	case PhysicsMode::FLYING:
		UpdatePhysicsFlying(fixedDeltaSeconds);
		break;
	case PhysicsMode::NOCLIP:
		UpdatePhysicsNoclip(fixedDeltaSeconds);
		break;
	}
}


void Entity::DebugDrawCollider() const
{
	std::vector<Vertex_PCU> verts;

	AddVertsForAABB3D(verts, AABB3(m_position - m_boxHalfExtents, m_position + m_boxHalfExtents), Rgba8::CYAN);
	Vec3 eyePos = GetEyePosition();
	AddVertsForAABB3D(verts, AABB3(eyePos - Vec3(0.1f, 0.1f, 0.1f), eyePos + Vec3(0.1f, 0.1f, 0.1f)), Rgba8::MAGENTA);

	DebugAddWorldWireTriangleListNoneCull(verts, 0.f, Rgba8::OPAQUE_WHITE, Rgba8::OPAQUE_WHITE, DebugRenderMode::X_RAY);

}

//-----------------------------------------------------------------------------------------------
void Entity::UpdatePhysicsWalking(float fixedDeltaSeconds)
{
	// Check if on ground
	m_isOnGround = IsGrounded();

	// Apply gravity if not on ground
	if (!m_isOnGround)
	{
		m_acceleration.z -= m_gravity;
	}

	// Apply horizontal drag (different for ground vs air)
	float currentDrag = m_isOnGround ? m_groundDrag : m_airDrag;

	AddForce(-currentDrag * Vec3(m_velocity.x, m_velocity.y, 0.f));

	// Update velocity from acceleration
	m_velocity += m_acceleration * fixedDeltaSeconds;
	m_acceleration = Vec3::ZERO;

	// Clamp Vertical Speed
	if (m_velocity.z < -15.f)
	{
		m_velocity.z = -15.f;
	}

	// Clamp horizontal speed
	//Vec2 horizontalVel(m_velocity.x, m_velocity.y);
	//float horizontalSpeed = horizontalVel.GetLength();
	//if (horizontalSpeed > m_maxWalkSpeed)
	//{
	//	horizontalVel = horizontalVel.GetNormalized() * m_maxWalkSpeed;
	//	m_velocity.x = horizontalVel.x;
	//	m_velocity.y = horizontalVel.y;
	//}

	// Calculate displacement for this frame
	Vec3 displacement = m_velocity * fixedDeltaSeconds;

	// Sweep and resolve collisions
	Vec3 finalDisplacement = SweepBox(displacement);
	m_position += finalDisplacement;
}

//-----------------------------------------------------------------------------------------------
void Entity::UpdatePhysicsFlying(float fixedDeltaSeconds)
{
	// No gravity in flying mode

	// Apply ground drag to all axes
	AddForce(-m_velocity * m_groundDrag);

	// Update velocity from acceleration
	m_velocity += m_acceleration * fixedDeltaSeconds;
	m_acceleration = Vec3::ZERO;

	// Clamp total speed
	//float speed = m_velocity.GetLength();
	//if (speed > m_maxFlySpeed)
	//{
	//	m_velocity = m_velocity.GetNormalized() * m_maxFlySpeed;
	//}

	// Calculate displacement for this frame
	Vec3 displacement = m_velocity * fixedDeltaSeconds;

	// Sweep and resolve collisions 
	Vec3 finalDisplacement = SweepBox(displacement);
	m_position += finalDisplacement;


	m_isOnGround = false;
}

//-----------------------------------------------------------------------------------------------
void Entity::UpdatePhysicsNoclip(float fixedDeltaSeconds)
{
	// No collision or gravity in noclip mode

	// Light drag
	AddForce(-m_velocity * m_groundDrag);

	// Update velocity from acceleration
	m_velocity += m_acceleration * fixedDeltaSeconds;
	m_acceleration = Vec3::ZERO;

	// Clamp speed
	//float speed = m_velocity.GetLength();
	//if (speed > m_maxFlySpeed)
	//{
	//	m_velocity = m_velocity.GetNormalized() * m_maxFlySpeed;
	//}

	// Direct position update, no collision detection
	m_position += m_velocity * fixedDeltaSeconds;

	m_isOnGround = false;
}

//-----------------------------------------------------------------------------------------------
void Entity::MoveInDirection(Vec3 const& direction, float speedMultiplier)
{
	// Choose max speed based on physics mode
	float targetSpeed = (m_physicsMode == PhysicsMode::WALKING) ? m_maxWalkSpeed : m_maxFlySpeed;

	// Choose appropriate drag based on physics mode and state
	float dragToUse = m_groundDrag;

	if (m_physicsMode == PhysicsMode::WALKING)
	{
		dragToUse = m_isOnGround ? m_groundDrag : m_airDrag;
	}
	else // FLYING or NOCLIP
	{
		dragToUse = m_groundDrag;
	}

	// Apply force to accelerate towards target speed
	AddForce(direction * speedMultiplier * targetSpeed * dragToUse);
}

//-----------------------------------------------------------------------------------------------
void Entity::AddForce(Vec3 const& force)
{
	m_acceleration += force;
}

//-----------------------------------------------------------------------------------------------
void Entity::AddImpulse(Vec3 const& impulse)
{
	m_velocity += impulse;
}

//-----------------------------------------------------------------------------------------------
void Entity::Jump()
{
	if (m_physicsMode == PhysicsMode::WALKING && m_isOnGround)
	{
		AddImpulse(Vec3(0.f, 0.f, m_jumpImpulse));
	}
}

std::string Entity::GetPhysicsModeName() const
{
	switch (m_physicsMode)
	{
	case PhysicsMode::WALKING:
		return "Walking";
		break;
	case PhysicsMode::FLYING:
		return "Flying";
		break;
	case PhysicsMode::NOCLIP:
		return "NoClip";
		break;
	default:
		return "";
		break;
	}
}

//-----------------------------------------------------------------------------------------------
bool Entity::IsGrounded() const
{
	if (m_physicsMode != PhysicsMode::WALKING)
		return false;

	// Check slightly below the entity
	float checkDistance = 0.1f;

	// Get the 4 bottom corners

	constexpr float SHRINK_AMOUNT = 0.02f;
	Vec3 bottomCorners[4] = {
		m_position + Vec3(-(m_boxHalfExtents.x - SHRINK_AMOUNT), -(m_boxHalfExtents.y - SHRINK_AMOUNT), -m_boxHalfExtents.z),
		m_position + Vec3(+(m_boxHalfExtents.x - SHRINK_AMOUNT), -(m_boxHalfExtents.y - SHRINK_AMOUNT), -m_boxHalfExtents.z),
		m_position + Vec3(-(m_boxHalfExtents.x - SHRINK_AMOUNT), +(m_boxHalfExtents.y - SHRINK_AMOUNT), -m_boxHalfExtents.z),
		m_position + Vec3(+(m_boxHalfExtents.x - SHRINK_AMOUNT), +(m_boxHalfExtents.y - SHRINK_AMOUNT), -m_boxHalfExtents.z),
	};

	// Start raycasts slightly inside the box to avoid edge issues
	float innerOffset = 0.01f;

	for (int i = 0; i < 4; ++i)
	{
		Vec3 rayStart = bottomCorners[i] + Vec3(0.f, 0.f, innerOffset);
		Vec3 rayDir = Vec3(0.f, 0.f, -1.f);

		BlockRaycastResult3D result = m_world->RaycastSolid(rayStart, rayDir, checkDistance + innerOffset);

		if (result.m_didImpact)
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------
void Entity::GetBoxCorners(std::vector<Vec3>& corners, float shrinkAmount /*= 0.f*/) const
{
	corners.clear();
	corners.reserve(8);

	// Generate all 8 corners of the bounding box
	for (int z = -1; z <= 1; z += 2)
	{
		for (int y = -1; y <= 1; y += 2)
		{
			for (int x = -1; x <= 1; x += 2)
			{
				Vec3 corner = m_position + Vec3(
					x * (m_boxHalfExtents.x - shrinkAmount),
					y * (m_boxHalfExtents.y - shrinkAmount),
					z * (m_boxHalfExtents.z - shrinkAmount)
				);
				corners.push_back(corner);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------
Vec3 Entity::SweepBox(Vec3 const& displacement)
{
	if (m_physicsMode == PhysicsMode::NOCLIP)
		return displacement;

	float epsilon = 0.001f;

	// Separate X, Y, Z movement for better collision response
	// Process in order: X -> Y -> Z
	Vec3 axes[3] = {
		Vec3(displacement.x, 0.f, 0.f),
		Vec3(0.f, displacement.y, 0.f),
		Vec3(0.f, 0.f, displacement.z)
	};

	Vec3 currentPos = Vec3::ZERO; // Result Displacement

	for (int axisIdx = 0; axisIdx < 3; ++axisIdx)
	{
		Vec3 axisMovement = axes[axisIdx];
		if (axisMovement.GetLengthSquared() < epsilon * epsilon)
			continue;

		float movementLength = axisMovement.GetLength();
		Vec3 movementDir = axisMovement.GetNormalized();

		// Determine which corners to check based on movement direction
		std::vector<Vec3> cornersToCheck;

		constexpr float SHRINK_AMOUNT = 0.02f;
		// Get all 8 corners (Shrink inside a litter)
		std::vector<Vec3> allCorners;
		GetBoxCorners(allCorners, SHRINK_AMOUNT);

		// Offset by current accumulated position
		for (Vec3& corner : allCorners)
		{
			corner += currentPos;
		}

		// Z Y X
		// Filter corners based on movement direction
		// Only check the 4 corners in the direction of movement
		for (int cornerIndex = 0; cornerIndex < 8; cornerIndex++)
		{
			Vec3 const& corner = allCorners[cornerIndex];
			bool shouldCheck = false;

			int bitMask = 1 << axisIdx;  // axisIdx=0 -> 1, axisIdx=1 -> 2, axisIdx=2 -> 4
			float axisMovementDir = (axisIdx == 0) ? movementDir.x :
				(axisIdx == 1) ? movementDir.y : movementDir.z;

			if ((axisMovementDir > 0 && (cornerIndex & bitMask) != 0) ||
				(axisMovementDir < 0 && (cornerIndex & bitMask) == 0))
			{
				shouldCheck = true;
			}

			if (shouldCheck)
			{
				cornersToCheck.push_back(corner);
			}
		}


		// Waist Check #ToDo According to m_boxHalfExtents, auto generate a grid with a gap < 1
		if ((axisIdx == 0 || axisIdx == 1) && m_boxHalfExtents.z > 0.5f)
		{
			// When moving in X or Y direction, we check the face perpendicular to movement
			// If Z is tall (> 1 voxel), add middle layer points at z = 0

			if (axisIdx == 0) // Moving in X direction, checking YZ plane
			{

				for (int y = -1; y <= 1; y += 2)
				{
					// Determine which X face to use based on movement direction
					int xSign = (movementDir.x > 0) ? 1 : -1;

					Vec3 middleCorner = m_position + currentPos + Vec3(
						xSign * (m_boxHalfExtents.x - SHRINK_AMOUNT),
						y * (m_boxHalfExtents.y - SHRINK_AMOUNT),
						0.f  // Middle Z
					);
					cornersToCheck.push_back(middleCorner);
				}
			}
			else // axisIdx == 1, Moving in Y direction, checking XZ plane
			{
				for (int x = -1; x <= 1; x += 2)
				{
					// Determine which Y face to use based on movement direction
					int ySign = (movementDir.y > 0) ? 1 : -1;

					Vec3 middleCorner = m_position + currentPos + Vec3(
						x * (m_boxHalfExtents.x - SHRINK_AMOUNT),
						ySign * (m_boxHalfExtents.y - SHRINK_AMOUNT),
						0.f  // Middle Z
					);
					cornersToCheck.push_back(middleCorner);
				}
			}
		}


		float rayLength = movementLength + SHRINK_AMOUNT; // IMPORTANT: end of the ray is the intended displacement 
		float earliestImpactDistance = rayLength;

		// Find earliest collision
		for (const Vec3& corner : cornersToCheck)
		{
			BlockRaycastResult3D result = m_world->RaycastSolid(corner, movementDir, rayLength);
			if (result.m_didImpact)
			{
				// #ToDo Draw 2 arrows
				DebugAddWorldArrow(result.m_rayStartPos, result.m_rayStartPos + result.m_impactNormal * 0.25f, 0.02f, 0.f, Rgba8::GREEN, Rgba8::GREEN, DebugRenderMode::X_RAY);


				if (result.m_impactDist < earliestImpactDistance)
				{
					earliestImpactDistance = result.m_impactDist;
				}
			}
		}

		float actualDist = (earliestImpactDistance - SHRINK_AMOUNT);

		// If we hit something, zero out velocity in that direction
		if (actualDist < movementLength * 0.98f)
		{
			if (axisIdx == 0) // X axis
			{
				m_velocity.x = 0.f;
			}
			else if (axisIdx == 1) // Y axis
			{
				m_velocity.y = 0.f;
			}
			else // Z axis
			{
				m_velocity.z = 0.f;
			}
		}


		Vec3 actualMovement = movementDir * actualDist;
		currentPos += actualMovement;
	}

	return currentPos;
}

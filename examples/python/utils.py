import pinocchio as pin


def add_floor_geom(geom_model: pin.GeometryModel) -> int:
    """Add a plane floor to the geometry.

    Returns the index for the new pin.GeometryObject.
    """
    from coal import Halfspace

    floor_collision_shape = Halfspace(0, 0, 1, 0)
    M = pin.SE3.Identity()
    floor_collision_object = pin.GeometryObject("floor", 0, 0, M, floor_collision_shape)
    floor_collision_object.meshColor[:] = 1.0
    return geom_model.addGeometryObject(floor_collision_object)

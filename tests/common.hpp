namespace am::tst {
    class CCamera {
    public:
        constexpr static auto fov = 80.0f;
        constexpr static auto p_near = 0.1f;
        constexpr static auto p_far = 256.0f;

        static CCamera make(const CInput* input) {
            AM_PROFILE_SCOPED();
            CCamera camera;
            camera._input = input;
            return camera;
        }

        void update(glm::vec2 viewport, float32 delta_time) noexcept {
            AM_PROFILE_SCOPED();
            _process_movement(delta_time);
            _aspect = viewport.x / viewport.y;
            _projection = glm::perspective(glm::radians(fov), _aspect, p_near, p_far);
            const auto cos_pitch = std::cos(glm::radians(_pitch));
            _front = glm::normalize(glm::vec3(
                std::sin(glm::radians(_yaw)) * -cos_pitch,
                std::sin(glm::radians(_pitch)),
                std::cos(glm::radians(_yaw)) * cos_pitch));
            _right = glm::normalize(glm::cross(_front, _world_up));
            _up = glm::normalize(glm::cross(_right, _front));
            _view = glm::lookAt(_position, _position + _front, _up);
            _generate_frustum();
        }

        glm::mat4 projection() const noexcept {
            AM_PROFILE_SCOPED();
            return _projection;
        }

        glm::mat4 view() const noexcept {
            AM_PROFILE_SCOPED();
            return _view;
        }

        glm::vec4 position() const noexcept {
            AM_PROFILE_SCOPED();
            return { _position, 1.0f };
        }

        float aspect() const noexcept {
            AM_PROFILE_SCOPED();
            return _aspect;
        }

        const glm::vec4 (&frustum() const noexcept)[6] {
            AM_PROFILE_SCOPED();
            return _frustum;
        }
    private:
        void _process_movement(float32 delta_time) noexcept {
            AM_PROFILE_SCOPED();
            constexpr auto camera_speed = 7.5f;
            const auto delta_movement = (camera_speed * delta_time);
            if (_input->is_key_pressed(Keyboard::kW)) {
                _position.x -= std::sin(glm::radians(_yaw)) * delta_movement;
                _position.z += std::cos(glm::radians(_yaw)) * delta_movement;
            }
            if (_input->is_key_pressed(Keyboard::kS)) {
                _position.x += std::sin(glm::radians(_yaw)) * delta_movement;
                _position.z -= std::cos(glm::radians(_yaw)) * delta_movement;
            }
            if (_input->is_key_pressed(Keyboard::kA)) {
                _position -= _right * delta_movement;
            }
            if (_input->is_key_pressed(Keyboard::kD)) {
                _position += _right * delta_movement;
            }
            if (_input->is_key_pressed(Keyboard::kSpace)) {
                _position += _world_up * delta_movement;
            }
            if (_input->is_key_pressed(Keyboard::kLeftShift)) {
                _position -= _world_up * delta_movement;
            }
            if (_input->is_key_pressed(Keyboard::kLeft)) {
                _yaw -= 150 * delta_time;
            }
            if (_input->is_key_pressed(Keyboard::kRight)) {
                _yaw += 150 * delta_time;
            }
            if (_input->is_key_pressed(Keyboard::kUp)) {
                _pitch += 150 * delta_time;
            }
            if (_input->is_key_pressed(Keyboard::kDown)) {
                _pitch -= 150 * delta_time;
            }
            if (_pitch > 89.9f) {
                _pitch = 89.9f;
            }
            if (_pitch < -89.9f) {
                _pitch = -89.9f;
            }
        }

        void _generate_frustum() noexcept {
            AM_PROFILE_SCOPED();
            const auto half_v = p_far * std::tanf(glm::radians(fov) * 0.5f);
            const auto half_h = half_v * _aspect;
            const auto front_far = p_far * _front;

            _frustum[0] = glm::make_vec4(glm::normalize(_front));
            _frustum[1] = glm::make_vec4(glm::normalize(-_front));
            _frustum[2] = glm::make_vec4(glm::normalize(glm::cross(_up, front_far + _right * half_h)));
            _frustum[3] = glm::make_vec4(glm::normalize(glm::cross(front_far - _right * half_h, _up)));
            _frustum[4] = glm::make_vec4(glm::normalize(glm::cross(_right, front_far - _up * half_v)));
            _frustum[5] = glm::make_vec4(glm::normalize(glm::cross(front_far + _up * half_v, _right)));

            _frustum[0].w = glm::dot(glm::vec3(_frustum[0]), _position + p_near * _front);
            _frustum[1].w = glm::dot(glm::vec3(_frustum[1]), _position + front_far);
            _frustum[2].w = glm::dot(glm::vec3(_frustum[2]), _position);
            _frustum[3].w = glm::dot(glm::vec3(_frustum[3]), _position);
            _frustum[4].w = glm::dot(glm::vec3(_frustum[4]), _position);
            _frustum[5].w = glm::dot(glm::vec3(_frustum[5]), _position);
        }

        glm::mat4 _projection = {};
        glm::mat4 _view = {};
        glm::vec3 _position = { 3.5f, 3.5f, 0.0f };
        glm::vec3 _front = { 0.0f, 0.0f, 1.0f };
        glm::vec3 _up = { 0.0f, 1.0f, 0.0f };
        glm::vec3 _right = { 0.0f, 0.0f, 0.0f };
        glm::vec3 _world_up = { 0.0f, 1.0f, 0.0f };
        glm::vec4 _frustum[6] = {};
        float32 _yaw = 90.0;
        float32 _pitch = 0.0;
        float32 _aspect = 0.0f;

        const CInput* _input = nullptr;
    };
} // namespace am
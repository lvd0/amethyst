namespace am::tst {
    class CCamera {
    public:
        constexpr static auto fov = 75.0f;
        constexpr static auto p_near = 0.1f;
        constexpr static auto p_far = 256.0f;

        static CCamera make(const CInput* input) {
            AM_PROFILE_SCOPED();
            CCamera camera;
            camera._input = input;
            return camera;
        }

        void update(float32 delta_time) noexcept {
            AM_PROFILE_SCOPED();
            _process_keyboard(delta_time);
            const auto* window = _input->window();
            _aspect = window->width() / (float32)window->height();
            _projection = glm::perspective(glm::radians(90.0f), _aspect, p_near, p_far);
            const auto cos_pitch = std::cos(glm::radians(_pitch));
            _front = glm::normalize(glm::vec3(
                std::cos(glm::radians(_yaw)) * cos_pitch,
                std::sin(glm::radians(_pitch)),
                std::sin(glm::radians(_yaw)) * cos_pitch
            ));
            _right = glm::normalize(glm::cross(_front, _world_up));
            _up = glm::normalize(glm::cross(_right, _front));
            _view = glm::lookAt(_position, _position + _front, _up);
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
    private:
        void _process_keyboard(float32 delta_time) noexcept {
            AM_PROFILE_SCOPED();
            constexpr auto camera_speed = 7.5f;
            const auto delta_movement = (camera_speed * delta_time);
            if (_input->is_key_pressed(Keyboard::kW)) {
                _position.x += std::cos(glm::radians(_yaw)) * delta_movement;
                _position.z += std::sin(glm::radians(_yaw)) * delta_movement;
            }
            if (_input->is_key_pressed(Keyboard::kS)) {
                _position.x -= std::cos(glm::radians(_yaw)) * delta_movement;
                _position.z -= std::sin(glm::radians(_yaw)) * delta_movement;
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

        glm::mat4 _projection;
        glm::mat4 _view;
        glm::vec3 _position = { 3.5f, 3.5f, 0.0f };
        glm::vec3 _front = { 0.0f, 0.0f, -1.0f };
        glm::vec3 _up = { 0.0f, 1.0f, 0.0f };
        glm::vec3 _right = { 0.0f, 0.0f, 0.0f };
        glm::vec3 _world_up = { 0.0f, 1.0f, 0.0f };
        float32 _yaw = -180.0;
        float32 _pitch = 0.0;
        float32 _aspect = 0.0f;

        const CInput* _input;
    };
} // namespace am